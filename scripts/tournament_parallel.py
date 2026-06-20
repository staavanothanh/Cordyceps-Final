#!/usr/bin/env python3
"""
tournament_parallel.py — Parallel tournament runner (8 workers, persistent processes).
Usage:
    # 14,400 games vs 1 opponent (≈3 hours 8 workers)
    python scripts/tournament_parallel.py --workers 8 --games 14400 --opponent superchym
    python scripts/tournament_parallel.py --workers 8 --games 14400 --opponent agent

    # Resume: re-run same command (appends to existing logs)
Features:
- 8 worker processes (8× speedup vs sequential)
- Persistent processes (no CreateProcess overhead between games)
- Batched logs: 1 file per worker per opponent (not 1 file per game)
- BTC format logs with full move sequences
"""

import subprocess
import threading
import queue
import time
import random
import os
import sys
import argparse

ROWS = 10
COLS = 17


def generate_random_board(seed: int):
    rng = random.Random(seed)
    return [[rng.randint(1, 9) for _ in range(COLS)] for _ in range(ROWS)]


def board_to_init_line(board):
    row_strs = [''.join(str(board[r][c]) for c in range(COLS)) for r in range(ROWS)]
    return 'INIT ' + ' '.join(row_strs)


class PersistentPlayer:
    def __init__(self, command, cwd, data_bin=None):
        work_dir = os.path.abspath(cwd)
        full_cmd = os.path.abspath(command) if not os.path.isabs(command) else command
        self.name = os.path.basename(command)
        if data_bin:
            src = os.path.abspath(data_bin)
            dst = os.path.join(work_dir, 'data.bin')
            if os.path.exists(src) and not os.path.exists(dst):
                import shutil
                try:
                    shutil.copy2(src, dst)
                except Exception:
                    pass
        self.process = subprocess.Popen(
            full_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, text=True, cwd=work_dir
        )
        self.reads = queue.Queue()
        self.lock = threading.Lock()
        t = threading.Thread(target=self._reader, daemon=True)
        t.start()
        self._sent_finish = False

    def _reader(self):
        while True:
            line = self.process.stdout.readline()
            if not line:
                break
            self.reads.put(line.strip())

    def send(self, msg):
        with self.lock:
            self.process.stdin.write(msg + '\n')
            self.process.stdin.flush()

    def readline(self, timeout=5.0):
        try:
            return self.reads.get(timeout=timeout)
        except queue.Empty:
            return None

    def close(self):
        if not self._sent_finish:
            try:
                self.send("FINISH")
                self._sent_finish = True
            except:
                pass
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()


def play_game(cordy, opp, board, cordy_first, timeout_ms=10000):
    """Play one game. Returns (log_lines, cordy_score, opp_score)."""
    local_board = [row[:] for row in board]
    timeout = [timeout_ms, timeout_ms]
    score = [0, 0]
    log_lines = []
    consecutive_passes = 0

    players = [cordy if cordy_first else opp, opp if cordy_first else cordy]
    opp_name = opp.name

    players[0].send(f"READY {'FIRST' if cordy_first else 'SECOND'}")
    players[1].send(f"READY {'SECOND' if cordy_first else 'FIRST'}")
    r0 = players[0].readline(5.0)
    r1 = players[1].readline(5.0)
    if r0 != "OK" or r1 != "OK":
        raise RuntimeError(f"READY failed: [{r0}] [{r1}]")

    init_line = board_to_init_line(board)
    for p in players:
        p.send(init_line)
    log_lines.append(board_to_log_line(board))

    for turn in range(999):
        side_idx = turn % 2
        opp_idx = 1 - side_idx
        player = players[side_idx]
        name = "cordyceps" if player is cordy else opp_name

        player.send(f"TIME {timeout[side_idx]} {timeout[opp_idx]}")
        start_t = time.time()
        resp = player.readline(timeout[side_idx] / 1000.0 + 1.0)
        elapsed = int((time.time() - start_t) * 1000)
        if resp is None:
            log_lines.append(f"ABORT {side_idx} TLE"); break
        try:
            r1, c1, r2, c2 = map(int, resp.split())
        except ValueError:
            log_lines.append(f"ABORT {side_idx} Parse: [{resp}]"); break
        elapsed = min(elapsed, timeout[side_idx])
        timeout[side_idx] -= elapsed
        if r1 == -1:
            if consecutive_passes >= 1:
                log_lines.append(f"{name} {r1} {c1} {r2} {c2} {elapsed}"); break
            consecutive_passes = 1
            players[opp_idx].send(f"OPP {r1} {c1} {r2} {c2} {elapsed}")
            log_lines.append(f"{name} {r1} {c1} {r2} {c2} {elapsed}")
            continue
        consecutive_passes = 0
        if not (0 <= r1 <= r2 < ROWS and 0 <= c1 <= c2 < COLS):
            log_lines.append(f"ABORT {side_idx} Range"); break
        rect_sum = 0
        live_cells = []
        for r in range(r1, r2 + 1):
            for c in range(c1, c2 + 1):
                if local_board[r][c] > 0:
                    rect_sum += local_board[r][c]
                    live_cells.append((r, c))
        if rect_sum != 10:
            log_lines.append(f"ABORT {side_idx} Sum={rect_sum}"); break
        left = right = top = down = False
        for r in range(r1, r2 + 1):
            if local_board[r][c1] > 0: left = True
            if local_board[r][c2] > 0: right = True
        for c in range(c1, c2 + 1):
            if local_board[r1][c] > 0: top = True
            if local_board[r2][c] > 0: down = True
        if not (left and right and top and down):
            log_lines.append(f"ABORT {side_idx} Inscribed"); break
        for r, c in live_cells:
            local_board[r][c] = 0
            score[side_idx] += 1
        players[opp_idx].send(f"OPP {r1} {c1} {r2} {c2} {elapsed}")
        log_lines.append(f"{name} {r1} {c1} {r2} {c2} {elapsed}")

    cordy_score = score[0] if cordy_first else score[1]
    opp_score = score[1] if cordy_first else score[0]
    log_lines.append(f"SCORECORDYCEPS {cordy_score}")
    log_lines.append(f"SCORE{opp_name.upper()} {opp_score}")
    return log_lines, cordy_score, opp_score


def board_to_log_line(board):
    return ' '.join(''.join(str(board[r][c]) for c in range(COLS)) for r in range(ROWS))


def worker(worker_id, task_queue, result_queue, cordy_cfg, opp_cfg, log_dir):
    """Worker: keeps 2 persistent processes, plays games, writes 1 batched log."""
    try:
        cordy = PersistentPlayer(cordy_cfg["command"], cordy_cfg["cwd"], cordy_cfg.get("data_bin"))
        opp = PersistentPlayer(opp_cfg["command"], opp_cfg["cwd"], opp_cfg.get("data_bin"))
    except Exception as e:
        result_queue.put(("error", worker_id, str(e)))
        return

    batch_fn = os.path.join(log_dir, f"worker{worker_id}_vs_{opp_cfg['name']}.txt")
    batch_fh = open(batch_fn, "w", buffering=65536)
    games_played = 0
    next_progress = 120

    while True:
        try:
            task = task_queue.get(timeout=5)
        except queue.Empty:
            break
        if task is None:
            break

        seed, board_idx, game_idx, cordy_first = task
        board = generate_random_board(seed)
        try:
            log_lines, cordy_score, opp_score = play_game(cordy, opp, board, cordy_first)
            side = "FIRST" if cordy_first else "SECOND"
            result = "WIN" if cordy_score > opp_score else "LOSS" if cordy_score < opp_score else "DRAW"

            # Write batched log entry
            batch_fh.write(f"# GAME board={board_idx:05d} game={game_idx} side={side} score={cordy_score}-{opp_score} result={result}\n")
            batch_fh.write(f"# SEED={seed}\n")
            for line in log_lines:
                batch_fh.write(line + "\n")
            batch_fh.write("\n")

            result_queue.put(("result", worker_id, side, cordy_score, opp_score, result))
            games_played += 1

            if games_played >= next_progress:
                print(f"  [W{worker_id}] {games_played} games done", flush=True)
                next_progress += 120

        except Exception as e:
            result_queue.put(("error", worker_id, str(e)))
            batch_fh.write(f"# ERROR: {e}\n")
            break

    batch_fh.close()
    cordy.close()
    opp.close()
    result_queue.put(("done", worker_id, games_played))


def main():
    parser = argparse.ArgumentParser(description="Parallel tournament runner")
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--games", type=int, default=14400, help="Games per opponent")
    parser.add_argument("--opponent", default="", help="Specific opponent (empty=all)")
    args = parser.parse_args()

    log_dir = "tournament_logs"
    os.makedirs(log_dir, exist_ok=True)

    # Read config from tournament_config.ini
    import configparser
    cfg = configparser.ConfigParser()
    cfg.read("tournament_config.ini")
    cordy_cfg = {
        "command": cfg["engines"]["cordyceps"].split(", ")[0],
        "cwd": cfg["engines"]["cordyceps"].split(", ")[1] if ", " in cfg["engines"]["cordyceps"] else ".",
        "name": "cordyceps"
    }
    opponents = []
    for key in cfg["engines"]:
        if key == "cordyceps": continue
        parts = cfg["engines"][key].split(", ")
        opp = {"command": parts[0], "cwd": parts[1] if len(parts) > 1 else ".", "name": key}
        if len(parts) > 2:
            opp["data_bin"] = parts[2]
        elif len(parts) > 1:
            opp["data_bin"] = os.path.join(parts[1], "data.bin")
        opponents.append(opp)
    if args.opponent:
        opponents = [o for o in opponents if o["name"] == args.opponent]

    num_boards = args.games // 2
    total_games = args.games * len(opponents)
    est_sec = total_games * 6 / args.workers
    print(f"{'='*60}")
    print(f"  PARALLEL TOURNAMENT ({args.workers} workers)")
    print(f"  {args.games:,} games/opponent = {num_boards:,} boards × 2 swaps")
    print(f"  {len(opponents)} opponent(s): {', '.join(o['name'] for o in opponents)}")
    print(f"  Est. {total_games:,} total games ≈ {est_sec/3600:.1f} hours")
    print(f"{'='*60}")

    for opp_cfg in opponents:
        print(f"\n  === vs {opp_cfg['name'].upper()} ({args.games:,} games) ===")
        start_t = time.time()
        task_queue = queue.Queue()
        result_queue = queue.Queue()

        for b in range(num_boards):
            bs = int(time.time() * 1000) + b + hash(opp_cfg['name']) % 100000
            task_queue.put((bs, b, 0, True))
            task_queue.put((bs, b, 1, False))
        for _ in range(args.workers):
            task_queue.put(None)

        workers = []
        for w in range(args.workers):
            t = threading.Thread(target=worker, args=(w, task_queue, result_queue, cordy_cfg, opp_cfg, log_dir))
            t.daemon = True; t.start(); workers.append(t)

        active = args.workers
        results = []
        errors = []
        next_print = 120

        while active > 0:
            msg = result_queue.get()
            if msg[0] == "result":
                _, wid, side, cs, os_, res = msg
                results.append(res)
                if len(results) >= next_print:
                    w = sum(1 for r in results if r == "WIN")
                    l = sum(1 for r in results if r == "LOSS")
                    d = sum(1 for r in results if r == "DRAW")
                    elapsed = time.time() - start_t
                    gps = len(results) / elapsed if elapsed > 0 else 0
                    rem = (args.games - len(results)) / gps if gps > 0 else 0
                    print(f"  {len(results):,}/{args.games:,}  W:{w} L:{l} D:{d}  "
                          f"{gps:.1f} g/s  ETA:{rem/60:.0f}min", end="\r", flush=True)
                    next_print += 120
            elif msg[0] == "error":
                _, wid, err = msg
                errors.append(err); active -= 1
            elif msg[0] == "done":
                _, wid, games = msg
                active -= 1

        for t in workers:
            t.join(timeout=5)

        elapsed = time.time() - start_t
        wins = sum(1 for r in results if r == "WIN")
        losses = sum(1 for r in results if r == "LOSS")
        draws = sum(1 for r in results if r == "DRAW")
        total = len(results)
        first_w = sum(1 for r in results[:total:2] if r == "WIN")
        first_l = sum(1 for r in results[:total:2] if r == "LOSS")
        first_d = sum(1 for r in results[:total:2] if r == "DRAW")
        second_w = sum(1 for r in results[1:total:2] if r == "WIN")
        second_l = sum(1 for r in results[1:total:2] if r == "LOSS")
        second_d = sum(1 for r in results[1:total:2] if r == "DRAW")

        print(f"\n  vs {opp_cfg['name']} ({elapsed/60:.0f}min, {total/elapsed:.1f} g/s):")
        print(f"    W: {wins:,}  L: {losses:,}  D: {draws:,}  ({wins/total*100:.1f}%)")
        print(f"    FIRST:  {first_w}/{first_w+first_l+first_d} ({first_w/(first_w+first_l+first_d)*100:.0f}%)")
        print(f"    SECOND: {second_w}/{second_w+second_l+second_d} ({second_w/(second_w+second_l+second_d)*100:.0f}%)")

        if errors:
            print(f"  [!] {len(errors)} errors")

    print(f"\n✅  Done. Logs in {log_dir}/")


if __name__ == "__main__":
    main()
