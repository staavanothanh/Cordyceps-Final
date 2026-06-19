#!/usr/bin/env python3
"""
tournament.py — Tournament runner cho Mushroom Game (NYPC format)
===============================================================
Usage:
    python scripts/tournament.py

Cấu hình trong tournament_config.ini:
--------------------------------------
[engines]
; Mỗi engine = 1 section riêng
; name = command, work_dir, data_bin (optional)

cordyceps = build/cordyceps.exe, .
agent = D:/NYPC-sample/agent-i-think-change/submissions/qr-submit-v1/agent.exe, D:/NYPC-sample/agent-i-think-change/submissions/qr-submit-v1, D:/NYPC-sample/agent-i-think-change/submissions/qr-submit-v1/data.bin

[tournament]
; Số ván mỗi cặp (mỗi board sẽ chơi 2 lần: swap)
games_per_board = 2
; Số board khác nhau
num_boards = 7
; Output log directory
log_dir = tournament_logs
; Seed cho random board generation
seed = 42

Format log BTC:
---------------
INIT <board_rows>
<enginename> <r1> <c1> <r2> <c2> <ms>
...
FINISH
SCORE<NAME1> <s1>
SCORE<NAME2> <s2>
"""

import subprocess
import threading
import queue
import time
import random
import os
import sys
import configparser
from pathlib import Path
from datetime import datetime


# ============================================================
# Board Utilities
# ============================================================

ROWS = 10
COLS = 17
CELLS = ROWS * COLS


def generate_random_board(seed: int) -> list[list[int]]:
    """Generate a random board with digits 1-9 (i.i.d. uniform)."""
    rng = random.Random(seed)
    board = [[0] * COLS for _ in range(ROWS)]
    for r in range(ROWS):
        for c in range(COLS):
            board[r][c] = rng.randint(1, 9)
    return board


def board_to_init_line(board: list[list[int]]) -> str:
    """Convert board to INIT format: 'INIT <10 row-strings>'"""
    row_strs = []
    for r in range(ROWS):
        row_str = ''.join(str(board[r][c]) for c in range(COLS))
        row_strs.append(row_str)
    return 'INIT ' + ' '.join(row_strs)


def board_to_log_line(board: list[list[int]]) -> str:
    """BTC-style INIT log line with row strings."""
    row_strs = [''.join(str(board[r][c]) for c in range(COLS)) for r in range(ROWS)]
    return ' '.join(row_strs)


# ============================================================
# Player Process Wrapper
# ============================================================

class Player:
    """Wraps an engine subprocess with stdin/stdout queues."""

    def __init__(self, command: str, cwd: str, data_bin: str | None = None):
        # Resolve paths
        work_dir = os.path.abspath(cwd)
        full_cmd = os.path.abspath(command) if not os.path.isabs(command) else command

        self.process = subprocess.Popen(
            full_cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            cwd=work_dir
        )
        self.reads: queue.Queue[str] = queue.Queue()
        self.name = os.path.basename(command)

        # Copy data.bin if specified
        if data_bin:
            data_path = os.path.abspath(data_bin)
            dest_path = os.path.join(work_dir, 'data.bin')
            if os.path.exists(data_path) and not os.path.exists(dest_path):
                try:
                    import shutil
                    shutil.copy2(data_path, dest_path)
                except Exception as e:
                    print(f"  [!] Cannot copy data.bin: {e}", file=sys.stderr)

        # Start stdout reader thread
        self._reader = threading.Thread(target=self._read_stdout, daemon=True)
        self._reader.start()

    def _read_stdout(self):
        while True:
            line = self.process.stdout.readline()
            if not line:
                break
            self.reads.put(line.strip())

    def send(self, msg: str):
        self.process.stdin.write(msg + '\n')
        self.process.stdin.flush()

    def readline(self, timeout: float = 5.0) -> str | None:
        try:
            return self.reads.get(timeout=timeout)
        except queue.Empty:
            return None

    def read_all_stale(self, timeout: float = 0.1) -> list[str]:
        """Read any available lines without blocking."""
        lines = []
        while True:
            try:
                lines.append(self.reads.get(timeout=timeout))
            except queue.Empty:
                break
        return lines

    def poll(self):
        return self.process.poll()

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()


# ============================================================
# Match Runner
# ============================================================

class MatchRunner:
    """
    Chạy 1 ván giữa 2 engines.
    Trả về log lines và scores.
    """

    def __init__(self, p1: Player, p2: Player, board: list[list[int]],
                 name1: str = "ENGINE1", name2: str = "ENGINE2",
                 timeout_ms: int = 10000):
        self.p1 = p1
        self.p2 = p2
        self.board = board
        self.name1 = name1
        self.name2 = name2
        self.timeout_ms = timeout_ms
        self.log_lines: list[str] = []

    def run(self) -> tuple[int, int]:
        """Play a match. Returns (score1, score2)."""
        # Track board state for sum=10 validation
        local_board = [row[:] for row in self.board]
        timeout = [self.timeout_ms, self.timeout_ms]
        score = [0, 0]
        passed = [False, False]
        us = [0, 1]
        names = [self.name1, self.name2]
        players = [self.p1, self.p2]
        consecutive_pass_count = 0

        # READY phase
        players[0].send(f"READY {names[0]}")
        players[1].send(f"READY {names[1]}")
        r0 = self.p1.readline(5.0)
        r1 = self.p2.readline(5.0)
        if r0 != "OK" or r1 != "OK":
            raise RuntimeError(f"READY failed: p1=[{r0}] p2=[{r1}]")

        # INIT phase
        init_line = board_to_init_line(self.board)
        for p in players:
            p.send(init_line)

        self.log_lines.append(board_to_log_line(self.board))

        # Game loop
        for turn in range(999):
            u = turn % 2
            opp = 1 - u
            player = players[u]
            name = names[u]
            opp_name = names[opp]

            # Send TIME
            player.send(f"TIME {timeout[u]} {timeout[opp]}")

            # Read response
            start_t = time.time()
            resp = player.readline(timeout[u] / 1000.0 + 1.0)
            elapsed_ms = int((time.time() - start_t) * 1000)

            if resp is None:
                self.log_lines.append(f"ABORT {u} TLE")
                break

            # Parse move
            try:
                r1, c1, r2, c2 = map(int, resp.split())
            except ValueError:
                self.log_lines.append(f"ABORT {u} Parse failed: [{resp}]")
                break

            elapsed_ms = min(elapsed_ms, timeout[u])
            timeout[u] -= elapsed_ms

            # Validate move
            if r1 == -1 and c1 == -1 and r2 == -1 and c2 == -1:
                # Pass
                if consecutive_pass_count >= 1:
                    # Both passed → game over
                    self.log_lines.append(f"{name} {r1} {c1} {r2} {c2} {elapsed_ms}")
                    break
                consecutive_pass_count = 1

                # Send OPP to opponent
                players[opp].send(f"OPP {r1} {c1} {r2} {c2} {elapsed_ms}")
                self.log_lines.append(f"{name} {r1} {c1} {r2} {c2} {elapsed_ms}")
                continue

            consecutive_pass_count = 0

            # Range check
            if not (0 <= r1 <= r2 < ROWS and 0 <= c1 <= c2 < COLS):
                self.log_lines.append(f"ABORT {u} Out of range: {r1} {c1} {r2} {c2}")
                break

            # Sum check: only live cells
            rect_sum = 0
            live_cells = []
            for r in range(r1, r2 + 1):
                for c in range(c1, c2 + 1):
                    if local_board[r][c] > 0:
                        rect_sum += local_board[r][c]
                        live_cells.append((r, c))

            if rect_sum != 10:
                self.log_lines.append(f"ABORT {u} Sum not equals to 10: got {rect_sum}")
                break

            # Inscribed check
            left = right = top = down = False
            for r in range(r1, r2 + 1):
                if local_board[r][c1] > 0: left = True
                if local_board[r][c2] > 0: right = True
            for c in range(c1, c2 + 1):
                if local_board[r1][c] > 0: top = True
                if local_board[r2][c] > 0: down = True
            if not (left and right and top and down):
                self.log_lines.append(f"ABORT {u} Not fit (inscribed)")
                break

            # Apply to local board
            for r, c in live_cells:
                local_board[r][c] = 0
                score[u] += 1

            # Send OPP to opponent
            players[opp].send(f"OPP {r1} {c1} {r2} {c2} {elapsed_ms}")
            self.log_lines.append(f"{name} {r1} {c1} {r2} {c2} {elapsed_ms}")

        # FINISH
        for p in players:
            p.send("FINISH")

        self.log_lines.append("FINISH")
        self.log_lines.append(f"SCORE{self.name1} {score[0]}")
        self.log_lines.append(f"SCORE{self.name2} {score[1]}")

        return score[0], score[1]


# ============================================================
# Config Loading
# ============================================================

def load_config(path: str = "tournament_config.ini") -> dict:
    """Load engine config from INI file."""
    config = configparser.ConfigParser()

    if os.path.exists(path):
        config.read(path)
    else:
        # Create default config
        config["engines"] = {
            "cordyceps": "build/cordyceps.exe, .",
            "agent": "D:/NYPC-sample/agent-i-think-change/submissions/qr-submit-v1/agent.exe, D:/NYPC-sample/agent-i-think-change/submissions/qr-submit-v1, D:/NYPC-sample/agent-i-think-change/submissions/qr-submit-v1/data.bin",
        }
        config["tournament"] = {
            "games_per_board": "2",
            "num_boards": "7",
            "log_dir": "tournament_logs",
            "seed": "42",
        }
        with open(path, "w") as f:
            config.write(f)

    result = {}

    # Parse engines
    engines = {}
    for name, val in config["engines"].items():
        parts = [p.strip() for p in val.split(",")]
        entry = {"command": parts[0], "cwd": parts[1] if len(parts) > 1 else "."}
        if len(parts) > 2 and parts[2]:
            entry["data_bin"] = parts[2]
        engines[name] = entry
    result["engines"] = engines

    # Parse tournament settings
    t = config["tournament"]
    result["games_per_board"] = int(t.get("games_per_board", "2"))
    result["num_boards"] = int(t.get("num_boards", "7"))
    result["log_dir"] = t.get("log_dir", "tournament_logs")
    result["seed"] = int(t.get("seed", "42"))

    return result


# ============================================================
# Tournament Runner
# ============================================================

def run_tournament(config: dict) -> None:
    """Run a tournament between cordyceps and all other engines."""
    engines = config["engines"]
    log_dir = config["log_dir"]
    games_per_board = config["games_per_board"]
    num_boards = config["num_boards"]
    base_seed = config["seed"]

    os.makedirs(log_dir, exist_ok=True)

    # Cordyceps is always the first engine
    if "cordyceps" not in engines:
        print("ERROR: 'cordyceps' engine not configured!", file=sys.stderr)
        sys.exit(1)

    cordyceps_cfg = engines["cordyceps"]

    # Run against every other engine
    for opp_name, opp_cfg in engines.items():
        if opp_name == "cordyceps":
            continue

        print(f"\n{'='*60}")
        print(f"  CORDYCEPS vs {opp_name.upper()}")
        print(f"{'='*60}")

        for board_idx in range(num_boards):
            seed = base_seed + board_idx
            board = generate_random_board(seed)

            # For each board, play games_per_board times (swap sides)
            for game_idx in range(games_per_board):
                swap = (game_idx % 2 == 1)
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                log_file = os.path.join(
                    log_dir,
                    f"cordyceps_vs_{opp_name}_board{board_idx:02d}_game{game_idx:02d}_{timestamp}.txt"
                )

                # Create fresh players
                p1_cfg = opp_cfg if swap else cordyceps_cfg
                p2_cfg = cordyceps_cfg if swap else opp_cfg
                n1 = opp_name if swap else "cordyceps"
                n2 = "cordyceps" if swap else opp_name

                p1 = Player(p1_cfg["command"], p1_cfg["cwd"],
                            p1_cfg.get("data_bin"))
                p2 = Player(p2_cfg["command"], p2_cfg["cwd"],
                            p2_cfg.get("data_bin"))

                try:
                    runner = MatchRunner(p1, p2, board, n1, n2)
                    s1, s2 = runner.run()

                    # Write log
                    with open(log_file, "w") as f:
                        f.write("\n".join(runner.log_lines) + "\n")
                        # Side info
                        if swap:
                            f.write(f"# CORDYCEPS_AS=SECOND\n")
                        else:
                            f.write(f"# CORDYCEPS_AS=FIRST\n")
                        f.write(f"# SEED={seed}\n")

                    # Determine cordyceps result
                    cordy_score = s2 if swap else s1
                    opp_score = s1 if swap else s2
                    result = "WIN" if cordy_score > opp_score else "LOSS" if cordy_score < opp_score else "DRAW"
                    side = "SECOND" if swap else "FIRST"

                    print(f"  [{board_idx+1}.{game_idx+1}] "
                          f"{side:6s}  Cordyceps {cordy_score:2d} - {opp_score:2d} {opp_name:20s}  [{result}]")

                except Exception as e:
                    print(f"  [{board_idx+1}.{game_idx+1}] ERROR: {e}", file=sys.stderr)
                finally:
                    p1.close()
                    p2.close()
                    time.sleep(0.1)  # Cool-down


# ============================================================
# Summary
# ============================================================

def print_summary(config: dict) -> None:
    """Print summary of all matches in log_dir."""
    log_dir = config["log_dir"]
    if not os.path.exists(log_dir):
        print("\nNo logs found.")
        return

    import glob
    files = sorted(glob.glob(os.path.join(log_dir, "*.txt")))

    results = {}  # opp_name → {"wins": 0, "losses": 0, "draws": 0, "first": {...}, "second": {...}}

    for f in files:
        with open(f) as fh:
            lines = fh.readlines()

        # Parse log
        scores = {}
        cordy_as = "FIRST"
        opp_name = "opponent"
        for line in lines:
            if line.startswith("SCORE"):
                parts = line.strip().split()
                if len(parts) == 2:
                    scores[parts[0][5:]] = int(parts[1])
            elif line.startswith("# CORDYCEPS_AS"):
                cordy_as = line.strip().split("=")[1]

        # Extract opponent name from filename: cordyceps_vs_<opp>_...
        base = os.path.basename(f)
        parts = base.split("_")
        if len(parts) >= 4:
            opp_name = parts[2]

        if opp_name not in results:
            results[opp_name] = {"wins": 0, "losses": 0, "draws": 0,
                                  "scores_as_first": [], "scores_as_second": []}

        # Determine cordyceps score
        cordy_score = None
        opp_score = None
        if "cordyceps" in scores:
            cordy_score = scores["cordyceps"]
        # Find non-cordyceps score
        for k, v in scores.items():
            if k != "cordyceps":
                opp_score = v
                break

        if cordy_score is not None and opp_score is not None:
            if cordy_score > opp_score:
                results[opp_name]["wins"] += 1
            elif cordy_score < opp_score:
                results[opp_name]["losses"] += 1
            else:
                results[opp_name]["draws"] += 1

            if cordy_as == "FIRST":
                results[opp_name]["scores_as_first"].append((cordy_score, opp_score))
            else:
                results[opp_name]["scores_as_second"].append((cordy_score, opp_score))

    # Print summary
    print(f"\n{'='*60}")
    print(f"  TOURNAMENT SUMMARY")
    print(f"{'='*60}")
    total_wins = total_losses = total_draws = 0
    total_first_wins = total_first_losses = 0
    total_second_wins = total_second_losses = 0
    total_first_score = total_second_score = 0
    total_first_games = total_second_games = 0

    for opp, r in sorted(results.items()):
        wins = r["wins"]
        losses = r["losses"]
        draws = r["draws"]
        total = wins + losses + draws
        winrate = wins / total * 100 if total else 0

        total_wins += wins
        total_losses += losses
        total_draws += draws

        # Per-side stats
        for side, scores_list in [("FIRST", r["scores_as_first"]),
                                   ("SECOND", r["scores_as_second"])]:
            if not scores_list:
                continue
            side_wins = sum(1 for c, o in scores_list if c > o)
            side_losses = sum(1 for c, o in scores_list if c < o)
            avg_cordy = sum(c for c, o in scores_list) / len(scores_list)
            avg_opp = sum(o for c, o in scores_list) / len(scores_list)

            if side == "FIRST":
                total_first_wins += side_wins
                total_first_losses += side_losses
                total_first_score += sum(c for c, o in scores_list) - sum(o for c, o in scores_list)
                total_first_games += len(scores_list)
            else:
                total_second_wins += side_wins
                total_second_losses += side_losses
                total_second_score += sum(c for c, o in scores_list) - sum(o for c, o in scores_list)
                total_second_games += len(scores_list)

        print(f"\n  vs {opp}:")
        print(f"    W: {wins:2d}  L: {losses:2d}  D: {draws:2d}  ({winrate:.0f}%)")

    total_games = total_wins + total_losses + total_draws
    if total_games:
        print(f"\n  {'─'*40}")
        print(f"  TOTAL: {total_games} games")
        print(f"    W: {total_wins}  L: {total_losses}  D: {total_draws}  "
              f"({total_wins/total_games*100:.0f}%)")

        if total_first_games:
            fw = total_first_wins / total_first_games * 100
            print(f"    As FIRST:  {total_first_wins}/{total_first_games} ({fw:.0f}%)  "
                  f"score_diff={total_first_score:+d}")
        if total_second_games:
            sw = total_second_wins / total_second_games * 100
            print(f"    As SECOND: {total_second_wins}/{total_second_games} ({sw:.0f}%)  "
                  f"score_diff={total_second_score:+d}")


# ============================================================
# Main
# ============================================================

def main():
    config = load_config("tournament_config.ini")

    print("🍄  CORDYCEPS TOURNAMENT RUNNER")
    print(f"    Log dir: {config['log_dir']}")
    print(f"    Boards:  {config['num_boards']}")
    print(f"    Games per board: {config['games_per_board']}")
    print(f"    Engines: {', '.join(k for k in config['engines'] if k != 'cordyceps')}")

    run_tournament(config)
    print_summary(config)

    print(f"\n✅  Tournament complete. Logs in {config['log_dir']}/")


if __name__ == "__main__":
    main()
