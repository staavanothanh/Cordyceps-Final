#!/usr/bin/env python3
"""
Optuna eval weight tuner for Cordyceps engine — DUAL-SIDE mode.
Tunes 14 params: 7 for FIRST + 7 for SECOND, simultaneously via self-play.

Usage:
  python tools/tune_optuna.py --trials 200 --games 8 --workers 8 --time 500

Resume (Ctrl+C → re-run same command):
  python tools/tune_optuna.py --trials 200 --games 8 --workers 8 --time 500

Features:
- In-process C++ tuner (no subprocess/IO overhead)
- 14-param dual-side tuning (7 FIRST + 7 SECOND)
- Resume via SQLite storage
- Multi-worker parallel evaluation
- Automatic best-weight persistence to best_weights.cfg
"""

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
TUNER_EXE = PROJECT / "build" / "tuner_cli.exe"

# Order: same as EvalWeights struct in board.hpp
WEIGHT_NAMES = ["score", "territory", "corners", "edges", "live_adj", "recapture", "vulnerability"]

# Per-weight ranges (min, max, default_from_baseline)
# Architect-verified from 4 reference engines (superchym, mushroom-bot,
# agent-i-think-change, old-cdc). Ranges account for feature diff magnitude
# (corner_diff max=4, live_adj_diff max=680, etc.)
WEIGHT_RANGES = {
    "score":         (0,  10, 3),
    "territory":     (0,  30, 3),
    "corners":       (0,  55, 8),
    "edges":         (0,  40, 2),
    "live_adj":      (-10, 70, 3),
    "recapture":     (0,  100, 0),
    "vulnerability": (-80, 35, 0),
}

BASELINE = [3, 3, 8, 2, 3, 0, 0]


def ensure_tuner():
    if not TUNER_EXE.exists():
        print("Building tuner_cli...")
        r = subprocess.run(
            ["cmake", "--build", str(PROJECT / "build"), "--target", "tuner_cli"],
            capture_output=True, text=True)
        if r.returncode != 0:
            print(f"Build failed: {r.stderr}", file=sys.stderr)
            sys.exit(1)


def run_tuner(weights_first, weights_second, games, seed, time_ms, timeout_s=600):
    """Run tuner_cli with dual weights. Returns (margin, wins, draws, losses, our_avg, opp_avg, score_diff, elapsed_ms)."""
    cmd = ([str(TUNER_EXE),
            "--weights-first"] + [str(x) for x in weights_first] +
           ["--weights-second"] + [str(x) for x in weights_second] +
           ["--games", str(games), "--seed", str(seed), "--time", str(time_ms)])
    try:
        r = subprocess.run(cmd, cwd=str(PROJECT), capture_output=True, text=True, timeout=timeout_s)
        if r.returncode != 0:
            return (-999.0, 0, 0, 0, 0.0, 0.0, 0, 0)
        parts = r.stdout.strip().split()
        return (
            float(parts[0]), int(parts[1]), int(parts[2]), int(parts[3]),
            float(parts[4]), float(parts[5]), int(parts[6]), int(parts[7]),
        )
    except subprocess.TimeoutExpired:
        return (-999.0, 0, 0, 0, 0.0, 0.0, 0, 0)
    except Exception:
        return (-999.0, 0, 0, 0, 0.0, 0.0, 0, 0)


def objective(trial, games, time_ms, base_seed, verbose=False):
    """Sample 14 weights, run tuner. Returns margin."""
    fw = []
    sw = []
    for name in WEIGHT_NAMES:
        lo, hi, default = WEIGHT_RANGES[name]
        fw.append(trial.suggest_int(f"f_{name}", lo, hi))
        sw.append(trial.suggest_int(f"s_{name}", lo, hi))

    seed = base_seed + trial.number
    result = run_tuner(fw, sw, games, seed, time_ms)
    margin, wins, draws, losses, our_avg, opp_avg, score_diff, elapsed = result

    trial.set_user_attr("wins", wins)
    trial.set_user_attr("draws", draws)
    trial.set_user_attr("losses", losses)
    trial.set_user_attr("our_avg", round(our_avg, 1))
    trial.set_user_attr("opp_avg", round(opp_avg, 1))
    trial.set_user_attr("score_diff", score_diff)
    trial.set_user_attr("elapsed_ms", elapsed)
    trial.set_user_attr("fw", fw)
    trial.set_user_attr("sw", sw)

    if verbose or trial.number % 10 == 0:
        total = wins + draws + losses
        s = (f"  #{trial.number:4d}: margin={margin:+.2f} "
             f"W{wins}D{draws}L{losses} ({our_avg:.1f}v{opp_avg:.1f}) "
             f"[{elapsed//1000}s]"
             f"  FW={fw[:3]}..  SW={sw[:3]}..")
        print(s, flush=True)

    return margin


def main():
    parser = argparse.ArgumentParser(description="Optuna eval weight tuner for Cordyceps (dual-side)")
    parser.add_argument("--trials", type=int, default=200, help="Optuna trials")
    parser.add_argument("--games", type=int, default=12, help="Games per trial (8=fast, 12=standard, 20=precise)")
    parser.add_argument("--workers", type=int, default=8, help="Parallel workers")
    parser.add_argument("--time", type=int, default=500, help="ms/move")
    parser.add_argument("--seed", type=int, default=42, help="Base seed")
    parser.add_argument("--study-name", default="cordyceps-weights-v2", help="Optuna study name")
    parser.add_argument("--storage", default=f"sqlite:///{PROJECT / 'optuna_study.db'}", help="Optuna storage URL")
    parser.add_argument("--verbose", action="store_true", help="Every trial")
    args = parser.parse_args()

    ensure_tuner()

    # Baseline measurement
    print("=" * 60)
    print("OPTUNA TUNE v2 — Cordyceps Dual-Side Eval Weights")
    print(f"  {args.trials} trials × {args.games} games  |  {args.workers} workers  |  {args.time}ms/move")
    print("  14 params: 7 FIRST + 7 SECOND")
    print("=" * 60)

    print("\n[1/3] Baseline (default weights)...")
    bl = run_tuner(BASELINE, BASELINE, args.games * 2, args.seed, args.time)
    baseline_margin = bl[0]
    print(f"  margin={baseline_margin:.2f}  W{bl[1]}D{bl[2]}L{bl[3]}  ({bl[4]:.1f}v{bl[5]:.1f})  [{bl[7]//1000}s]")

    per_game_ms = bl[7] / max(args.games * 2, 1)
    per_trial_ms = per_game_ms * args.games
    est_s = per_trial_ms * args.trials / max(args.workers, 1) / 1000
    print(f"\n  Est. time: ~{est_s/60:.0f} min ({est_s/3600:.2f}h)")

    # Optuna
    print(f"\n[2/3] Optuna ({args.trials} trials, 14 params)...")
    import optuna
    from optuna.samplers import TPESampler

    study = optuna.create_study(
        direction="maximize",
        study_name=args.study_name,
        storage=args.storage,
        load_if_exists=True,
        sampler=TPESampler(seed=args.seed, multivariate=True),
    )

    study.optimize(
        lambda t: objective(t, args.games, args.time, args.seed, args.verbose),
        n_trials=args.trials,
        n_jobs=args.workers,
        show_progress_bar=True,
    )

    # Results
    print("\n" + "=" * 60)
    bt = study.best_trial
    print(f"Best: trial #{bt.number}  margin={bt.value:.2f}  (baseline={baseline_margin:.2f})")
    print(f"  Delta: {bt.value - baseline_margin:+.2f}")

    fw = [int(bt.params.get(f"f_{n}", WEIGHT_RANGES[n][2])) for n in WEIGHT_NAMES]
    sw = [int(bt.params.get(f"s_{n}", WEIGHT_RANGES[n][2])) for n in WEIGHT_NAMES]
    print(f"  FIRST  weights: {fw}")
    print(f"  SECOND weights: {sw}")

    verify = run_tuner(fw, sw, 8, 42, args.time)
    print(f"  Verify margin: {verify[0]:+.2f}  W{verify[1]}D{verify[2]}L{verify[3]}")

    # Write best_weights.cfg
    best_path = PROJECT / "best_weights.cfg"
    with open(best_path, "w") as f:
        f.write("# Cordyceps best eval weights (from Optuna v2 dual-side tuning)\n")
        f.write(f"# margin={bt.value:.2f} (baseline={baseline_margin:.2f}) verify={verify[0]:.2f}\n")
        f.write("#\n# To use: set env var WEIGHTS_CFG=best_weights.cfg or copy weights into code\n\n")
        f.write("# FIRST weights (when engine plays as FIRST)\n")
        f.write(f"FIRST={' '.join(str(x) for x in fw)}\n\n")
        f.write("# SECOND weights (when engine plays as SECOND)\n")
        f.write(f"SECOND={' '.join(str(x) for x in sw)}\n\n")
        f.write("# Single-line for tuner_cli --weights (FIRST weights, backward compat)\n")
        f.write(" ".join(str(x) for x in fw) + "\n")

    print(f"\n[3/3] Best weights written to {best_path}")
    print()
    print("To run with tuned weights:")
    print("  set WEIGHTS_CFG=best_weights.cfg")
    print()
    print("To resume tuning: run the same command again")
    print("=" * 60)


if __name__ == "__main__":
    main()
