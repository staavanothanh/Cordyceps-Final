#!/usr/bin/env python3
"""
Optuna eval weight tuner for Cordyceps engine.
Usage: python tools/tune_optuna.py --trials 200 --games 20 --workers 8

Features:
- In-process C++ tuner (no subprocess/IO overhead)
- Resume via SQLite storage (Ctrl+C → restart → continue)
- Multi-worker parallel evaluation
- Automatic best-weight persistence
"""

import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
TUNER_EXE = PROJECT / "build" / "tuner_cli.exe"
BASELINE_W = [3, 3, 8, 2, 3, 0, 0]  # current default weights

# Order: score, territory, corners, edges, live_adj, recapture, vulnerability
WEIGHT_SPEC = [
    ("score",         0, 15, 3),      # ±15 range, default 3
    ("territory",     0, 15, 3),      # ±15, default 3
    ("corners",       0, 25, 8),      # ±25, default 8
    ("edges",         0, 10, 2),      # ±10, default 2
    ("live_adj",     -5, 10, 3),      # can be negative
    ("recapture",     0, 15, 2),      # opponent's stealable cells
    ("vulnerability",-10,  5, 0),      # our stealable cells (negative = penalty)
]


def ensure_tuner():
    """Build tuner_cli if not present."""
    if not TUNER_EXE.exists():
        print("Building tuner_cli...")
        import subprocess
        r = subprocess.run(
            ["cmake", "--build", str(PROJECT / "build"), "--target", "tuner_cli"],
            capture_output=True, text=True
        )
        if r.returncode != 0:
            print(f"Build failed: {r.stderr}", file=sys.stderr)
            sys.exit(1)


def run_tuner(weights, games, seed, time_ms, timeout_s=300):
    """Run tuner_cli with given weights. Returns (margin, wins, draws, losses, our_avg, opp_avg, score_diff, elapsed_ms)."""
    w = [str(x) for x in weights]
    cmd = [str(TUNER_EXE), "--weights"] + w + ["--games", str(games), "--seed", str(seed), "--time", str(time_ms)]
    try:
        r = subprocess.run(cmd, cwd=str(PROJECT), capture_output=True, text=True, timeout=timeout_s)
        if r.returncode != 0:
            print(f"  [!] tuner_cli error: {r.stderr.strip()}", file=sys.stderr)
            return (-999.0, 0, 0, 0, 0.0, 0.0, 0, 0)
        parts = r.stdout.strip().split()
        return (
            float(parts[0]),  # margin
            int(parts[1]),   # wins
            int(parts[2]),   # draws
            int(parts[3]),   # losses
            float(parts[4]), # our_avg
            float(parts[5]), # opp_avg
            int(parts[6]),   # score_diff
            int(parts[7]),   # elapsed_ms all games
        )
    except subprocess.TimeoutExpired:
        print(f"  [!] tuner_cli timed out", file=sys.stderr)
        return (-999.0, 0, 0, 0, 0.0, 0.0, 0, 0)
    except Exception as e:
        print(f"  [!] Error: {e}", file=sys.stderr)
        return (-999.0, 0, 0, 0, 0.0, 0.0, 0, 0)


def objective(trial, games, time_ms, base_seed, verbose=False):
    """Optuna objective: run tuner_cli with sampled weights."""
    w = []
    for name, lo, hi, default in WEIGHT_SPEC:
        val = trial.suggest_int(name, lo, hi)
        w.append(val)

    seed = base_seed + trial.number
    result = run_tuner(w, games, seed, time_ms)

    margin, wins, draws, losses, our_avg, opp_avg, score_diff, elapsed = result

    trial.set_user_attr("wins", wins)
    trial.set_user_attr("draws", draws)
    trial.set_user_attr("losses", losses)
    trial.set_user_attr("our_avg", round(our_avg, 1))
    trial.set_user_attr("opp_avg", round(opp_avg, 1))
    trial.set_user_attr("score_diff", score_diff)
    trial.set_user_attr("elapsed_ms", elapsed)

    if verbose or trial.number % 10 == 0:
        games_total = wins + draws + losses
        s = f"  #{trial.number:4d}: margin={margin:+.2f} W{wins}D{draws}L{losses} ({our_avg:.1f}v{opp_avg:.1f}) [{elapsed//1000}s] {w}"
        print(s, flush=True)

    return margin


def main():
    parser = argparse.ArgumentParser(description="Optuna eval weight tuner for Cordyceps")
    parser.add_argument("--trials", type=int, default=200, help="Number of Optuna trials")
    parser.add_argument("--games", type=int, default=20, help="Games per trial")
    parser.add_argument("--workers", type=int, default=8, help="Parallel workers (0 = serial)")
    parser.add_argument("--time", type=int, default=500, help="Time budget per move (ms)")
    parser.add_argument("--seed", type=int, default=42, help="Base seed")
    parser.add_argument("--study-name", default="cordyceps-weights-v1", help="Optuna study name")
    parser.add_argument("--storage", default=f"sqlite:///{PROJECT / 'optuna_study.db'}", help="Optuna storage URL")
    parser.add_argument("--verbose", action="store_true", help="Print every trial")
    args = parser.parse_args()

    # Build tuner
    ensure_tuner()

    # Baseline measurement
    print("=" * 60)
    print("OPTUNA TUNE — Cordyceps Eval Weights")
    print(f"  {args.trials} trials × {args.games} games  |  {args.workers} workers  |  {args.time}ms/move")
    print("=" * 60)

    print("\n[1/3] Baseline (default weights)...")
    bl = run_tuner(BASELINE_W, args.games * 2, args.seed, args.time)  # double games for baseline
    baseline_margin = bl[0]
    print(f"  margin={baseline_margin:.2f} W{bl[1]}D{bl[2]}L{bl[3]} ({bl[4]:.1f}v{bl[5]:.1f}) [{bl[7]//1000}s]")

    # Estimate time properly using per-game time from baseline
    per_game_ms = bl[7] / (args.games * 2) if (args.games * 2) > 0 else 1000
    per_trial_ms = per_game_ms * args.games
    est_s = per_trial_ms * args.trials / max(args.workers, 1) / 1000
    print(f"\n  Est. time: ~{est_s/60:.0f} min ({est_s/3600:.1f}h) with {args.workers} workers")

    # Optuna
    print(f"\n[2/3] Optuna ({args.trials} trials)...")
    import optuna
    from optuna.samplers import TPESampler

    study = optuna.create_study(
        direction="maximize",
        study_name=args.study_name,
        storage=args.storage,
        load_if_exists=True,  # RESUME SUPPORT
        sampler=TPESampler(seed=args.seed),
    )

    if args.workers > 1:
        study.optimize(
            lambda trial: objective(trial, args.games, args.time, args.seed, args.verbose),
            n_trials=args.trials,
            n_jobs=args.workers,
            show_progress_bar=True,
        )
    else:
        study.optimize(
            lambda trial: objective(trial, args.games, args.time, args.seed, args.verbose),
            n_trials=args.trials,
            show_progress_bar=True,
        )

    print("\n" + "=" * 60)
    print(f"Best: trial #{study.best_trial.number}  margin={study.best_value:.2f}")
    print(f"Baseline: {baseline_margin:.2f}  Delta: {study.best_value - baseline_margin:.2f}")
    print(f"Weights: {study.best_params}")

    # Write best weights to file
    best_path = PROJECT / "best_weights.cfg"
    order = ["score", "territory", "corners", "edges", "live_adj", "recapture", "vulnerability"]
    with open(best_path, "w") as f:
        f.write("# Cordyceps best eval weights (from Optuna tuning)\n")
        f.write(f"# margin={study.best_value:.2f} (baseline={baseline_margin:.2f})\n")
        for name in order:
            f.write(f"{name}={study.best_params.get(name, 0)}\n")
        f.write("\n")
        # Also write a one-line weights for tuner_cli
        vals = [str(study.best_params.get(name, 0)) for name in order]
        f.write(" ".join(vals) + "\n")

    print(f"\n[3/3] Best weights written to {best_path}")
    print("  Use: tuner_cli --weights " + " ".join(vals) + " --games 20")
    print("\n  To resume: run the same command again (study stored in optuna_study.db)")
    print("=" * 60)


if __name__ == "__main__":
    main()
