# DESIGN: 14-Parameter Self-Play Tuner (7 FIRST + 7 SECOND)

> **Author**: Senior Software Architect  
> **Date**: 2026-07-14  
> **Status**: Draft for review  
> **Target**: Cordyceps engine — eval weight tuning pipeline

---

## Table of Contents

1. [Current Architecture & Limitations](#1-current-architecture--limitations)
2. [Design: tuner_cli.cpp Upgrade](#2-design-tuner_clicpp-upgrade)
3. [Design: tune_optuna.py Upgrade](#3-design-tune_optunapy-upgrade)
4. [Range Justification](#4-range-justification)
5. [Trade-Off Analysis](#5-trade-off-analysis)
6. [Statistical Significance & Required Games](#6-statistical-significance--required-games)
7. [Implementation Plan](#7-implementation-plan)
8. [Appendix: Complete File Schemas](#8-appendix-complete-file-schemas)

---

## 1. Current Architecture & Limitations

### Current Flow

```
tune_optuna.py
  └─ subprocess → tuner_cli --weights w0..w6 --games N --seed S --time MS
                    └─ in-process: play_game(board, candidate_weights, candidate_is_first)
                       └─ set_tune_weights(w0..w6)  ← SAME weights for both sides
                       └─ clear_tune_weights()      ← baseline (compile-time constants)
```

### Key Limitations

| Issue | Detail | Impact |
|-------|--------|--------|
| **1-weight set** | Same 7 weights used for both FIRST and SECOND | Cannot capture asymmetric play — FIRST needs aggression, SECOND needs defense |
| **200ms budget** | Depth 5-6, noisy evaluation | Signal-to-noise ratio too low; 500ms gives depth 7-8 |
| **Global thread-local** | `g_tune_w0..w6` are single set | Cannot hold FIRST+SECOND weights simultaneously |
| **No validation split** | All boards used for tuning | No way to detect overfitting |
| **Rudimentary scoring** | Board-level `candidate_score - opponent_score` | Doesn't track accumulated margin across full game set |
| **No early stopping** | Runs full 200 trials even if plateaued | Wastes compute |

---

## 2. Design: tuner_cli.cpp Upgrade

### 2.1 Interface Change

```diff
- tuner_cli --weights w0 w1 w2 w3 w4 w5 w6 --games N --seed S --time MS
+ tuner_cli --weights-first w0 w1 w2 w3 w4 w5 w6
+           --weights-second w0 w1 w2 w3 w4 w5 w6
+           --games N --seed S --time MS
```

Or, more compactly, accept 14 params in positional order:

```
tuner_cli --weights-fw0 fw1 fw2 fw3 fw4 fw5 fw6 sw0 sw1 sw2 sw3 sw4 sw5 sw6
          --games N --seed S --time MS
```

**Recommendation**: Use named `--weights-first` and `--weights-second` for clarity in Python-side logging, but also support a combined 14-param version for minimal argument count.

### 2.2 Weight Storage (board.cpp extension)

Add a **second** set of tune weights to support simultaneous FIRST/SECOND:

```cpp
// board.cpp — extend thread-local storage
// Order: score, territory, corners, edges, live_adj, recapture, vulnerability
static thread_local int g_tune_fw0 = 0;  // FIRST weights
static thread_local int g_tune_fw1 = 0;
// ...
static thread_local int g_tune_fw6 = 0;
static thread_local int g_tune_sw0 = 0;  // SECOND weights
static thread_local int g_tune_sw1 = 0;
// ...
static thread_local int g_tune_sw6 = 0;
static thread_local bool g_tune_active = false;

void set_tune_weights_first(int score_w, int territory_w, int corner_w, int edge_w,
                            int adj_w, int recapture_w, int vulnerability_w) noexcept;
void set_tune_weights_second(int score_w, int territory_w, int corner_w, int edge_w,
                             int adj_w, int recapture_w, int vulnerability_w) noexcept;
void clear_tune_weights() noexcept;
```

**ALERT**: This changes the global tune API. All existing callers (`tuner_cli.cpp`, tests) must be updated.

### 2.3 evaluate() Extension

The `evaluate()` function needs to know **which side** is being evaluated to select the right weight set:

```cpp
int evaluate(const Board& board, int player) noexcept {
    // ... compute diffs as before ...
    
    if (g_tune_active) {
        // FIRST weights when player == k_player_us
        // SECOND weights when player == k_player_opp
        const int* w = (player == k_player_us) ? g_tune_f_ : g_tune_s_;
        // But wait — the tuner game logic calls evaluate for both sides.
        // The "candidate" can be FIRST or SECOND depending on which side of the board.
        // We need the candidate's weights to be used when it's candidate's turn,
        // and baseline weights when it's baseline's turn.
    }
}
```

**Better approach**: Don't change `evaluate()` at all. Instead, in the `play_game()` loop, swap which global set is active:

```cpp
// In play_game(): candidate plays with candidate_is_first flag
if (is_candidate_turn) {
    if (candidate_is_first) {
        set_tune_weights_first(candidate_fw[0..6]);  // Candidate FIRST weights
        set_tune_weights_second(baseline_sw[0..6]);   // Baseline SECOND weights
    } else {
        set_tune_weights_first(baseline_fw[0..6]);    // Baseline FIRST weights  
        set_tune_weights_second(candidate_sw[0..6]);  // Candidate SECOND weights
    }
} else {
    [same logic but with roles reversed]
}
```

Wait, this is getting complex. Let me reconsider the architecture.

### 2.4 Simpler Approach: Evaluate Uses Player-Aware Weight Selection

Instead of having two separate calls, have a single `set_tune_weights` that takes ALL 14 weights, and `evaluate` picks the correct subset based on `player`:

```cpp
// board.cpp
static thread_local int g_tune_w0[2];  // [0] = FIRST weights, [1] = SECOND weights
// ... for all 7 params
static thread_local bool g_tune_active = false;

// Called once before search
void set_tune_weights_14(
    int f_score, int f_territory, int f_corners, int f_edges, int f_live_adj, int f_recapture, int f_vulnerability,
    int s_score, int s_territory, int s_corners, int s_edges, int s_live_adj, int s_recapture, int s_vulnerability
) noexcept {
    g_tune_w0[k_player_us] = f_score;    // index 0 = FIRST
    g_tune_w1[k_player_us] = f_territory;
    // ...
    g_tune_w0[k_player_opp] = s_score;   // index 1 = SECOND
    g_tune_w1[k_player_opp] = s_territory;
    // ...
    g_tune_active = true;
}

int evaluate(const Board& board, int player) noexcept {
    // ... compute diffs ...
    
    if (g_tune_active) {
        int idx = (player == k_player_us) ? 0 : 1;
        return score * g_tune_w0[idx]
             + territory_diff * g_tune_w1[idx]
             + corner_diff * g_tune_w2[idx]
             + edge_diff * g_tune_w3[idx]
             + adj_diff * g_tune_w4[idx]
             + /* conn_diff * 0 — not tuned */
             ;
    }
    
    // Baseline
    return score * 3 + territory_diff * 3 + corner_diff * 8 + edge_diff * 2 + adj_diff * 3;
}
```

This way, `evaluate()` automatically picks the right weights based on which player is being evaluated. The search calls `evaluate(board, board.current_player)` which alternates as players take turns, so:

- When candidate plays FIRST: `board.current_player` alternates, but the search evaluates from that player's perspective. With our 14-param setup, `evaluate(board, k_player_us)` uses FIRST weights (index 0), and `evaluate(board, k_player_opp)` uses SECOND weights (index 1).

Wait, but the candidate uses the same weights throughout the game regardless of which side they play. Let me re-examine this...

Actually, the issue is more subtle. When the candidate plays FIRST:
- On candidate's turn: `board.current_player == k_player_us` (FIRST), evaluate uses FIRST weights — good.
- On opponent's (baseline) turn: `board.current_player == k_player_opp` (SECOND), evaluate uses SECOND weights.

But what should SECOND weights be? The baseline (opponent) should use the DEFAULT (compile-time) weights, not the tuned SECOND weights! The candidate's SECOND weights should only apply when the candidate is playing SECOND.

So the approach needs to be:

**Each `play_game` call sets weights based on who the candidate is AND whose turn it is.**

Let me think about this differently. There are 4 possible weight assignments:

| Scenario | Candidate is | Current turn | Candidate's weights apply? | Baseline's weights apply? |
|----------|-------------|-------------|---------------------------|--------------------------|
| A | FIRST | Candidate (FIRST) | Yes (FIRST weights) | No |
| B | FIRST | Baseline (SECOND) | No | Yes (baseline defaults) |
| C | SECOND | Candidate (SECOND) | Yes (SECOND weights) | No |
| D | SECOND | Baseline (FIRST) | No | Yes (baseline defaults) |

So the simplest approach: **`play_game` takes candidate weights AND which side the candidate plays, and switches between candidate weights and baseline (cleared) weights at each turn.**

```cpp
GameResult play_game(const RectTable& table, const Zobrist& zobrist,
                     Board board, int time_ms,
                     const int* candidate_fw,   // 7 FIRST weights for candidate
                     const int* candidate_sw,   // 7 SECOND weights for candidate
                     bool candidate_is_first) noexcept {
    
    bool is_candidate_turn = candidate_is_first;
    
    for (int turn = 0; turn < 200; ++turn) {
        if (board.is_terminal()) break;
        
        if (is_candidate_turn) {
            // Candidate's turn — use candidate weights for THIS side
            if (candidate_is_first) {
                set_tune_weights(candidate_fw[0], ...);  // FIRST weights
            } else {
                set_tune_weights(candidate_sw[0], ...);  // SECOND weights
            }
        } else {
            // Baseline's turn — use default compile-time weights
            clear_tune_weights();
        }
        
        SearchResult result = searcher.iterative_deepening(board, time_ms, side);
        // ...
        
        is_candidate_turn = !is_candidate_turn;
    }
}
```

This is actually simpler and doesn't require changing `evaluate()` at all — just extends the existing `set_tune_weights`/`clear_tune_weights` pattern. When `clear_tune_weights()` is called, evaluate uses the hardcoded baseline weights.

**BUT** — there's a subtlety: during search (negamax), `evaluate` is called at leaf nodes for the side-to-move. When the baseline is on turn, we want the baseline's weights to be used for all evaluations in that search tree. When the candidate is on turn, we want the candidate's weights used. The current pattern of calling `set_tune_weights`/`clear_tune_weights` BEFORE `searcher.iterative_deepening` handles this correctly because the tune weights are set for the duration of that search call.

Wait, but the search does `evaluate(board, board.current_player)` at leaf nodes. The current player alternates in the search tree via negamax. So during a single search call, the current_player changes as moves are applied/unapplied. The tune weights are the same regardless of whose perspective the evaluation is from. So this is fine — all leaf evaluations in a single search use the same tune weights (since the tune_w globals don't change mid-search).

Yes, this works. The key insight: **a single search call evaluates all leaf nodes from the perspective of the side-to-move at that node, always using the currently set tune weights.** Since we set tune weights before calling search, all evaluations within that search use those weights.

Great, so the simple approach works:

```cpp
// Before candidate's search:
set_tune_weights(candidate_fw[0], ...);  // For candidate-as-FIRST
// Or
set_tune_weights(candidate_sw[0], ...);  // For candidate-as-SECOND

// Before baseline's search:
clear_tune_weights();
```

### 2.5 CLI Interface

```
tuner_cli --weights-first w0 w1 w2 w3 w4 w5 w6
          --weights-second w0 w1 w2 w3 w4 w5 w6
          --games N --seed S --time MS
```

**Output (single line to stdout)**:

```
margin wins draws losses our_avg opp_avg score_diff elapsed_ms
```

Same format as before — backward compatible. The Python script parses this identically.

### 2.6 Game Logic (4 games per trial)

```cpp
void evaluate_weights_14(const int* fw, const int* sw,  // 7 FIRST + 7 SECOND weights
                         int num_boards, uint64_t base_seed, int time_ms) {
    // num_boards = games/4 (since each board generates 4 games)
    // But to be clean: num_boards is the number of unique board positions
    
    for each board:
        // Game 1: Candidate FIRST vs Baseline SECOND
        play_game(board, candidate_fw=fw, candidate_sw=sw, candidate_is_first=true)
        
        // Game 2: Candidate SECOND vs Baseline FIRST (same board, swap sides)
        // Candidate is SECOND, baseline is FIRST
        // On candidate's turn: use sw (SECOND weights)
        // On baseline's turn: clear_tune_weights()
        play_game(board, candidate_fw=fw, candidate_sw=sw, candidate_is_first=false)
        
        // Game 3 & 4: Same two games but with swapped seed (different rollouts)
        // Different board from same base_seed to add noise diversity
        Board board2 = generate_board(splitmix64(seed + 1000000));  // different board
        
        play_game(board2, candidate_fw=fw, candidate_sw=sw, candidate_is_first=true)
        play_game(board2, candidate_fw=fw, candidate_sw=sw, candidate_is_first=false)
    
    // Accumulate all candidate_scores - opponent_scores across 4 games
    // This is the "margin" reported
}
```

### 2.7 Score Tracking (Accumulated Margin, NOT board scores)

The current code tracks:
```cpp
if (!best.is_pass()) {
    int gained = 0;
    for (int r = best.r1; r <= best.r2; ++r)
        for (int c = best.c1; c <= best.c2; ++c)
            if (board.value_at(r, c) > 0) gained++;
    
    if (is_candidate_turn) candidate_score += gained;
    else opponent_score += gained;
}
```

This counts cells captured per move. The **accumulated margin** = sum of (candidate_score - opponent_score) across all 4 games. This is correct and independent of `board.my_score`/`board.opp_score`.

**Key**: The margin is computed as `candidate_score - opponent_score` from the per-move cell counting, NOT from the board's internal score tracking. This is already correct in the current code.

### 2.8 SideConfig Handling

Currently `SideConfig side;` is default-initialized (all zeros/false). The tuner doesn't set config parameters. For the 14-param design, we might want to pass different aggression levels for FIRST vs SECOND, but that's out of scope for now. Keep using default `SideConfig{}`.

---

## 3. Design: tune_optuna.py Upgrade

### 3.1 Architecture

```
tune_optuna.py
  │
  ├──> spawn pool of 8 worker processes
  │     └── each worker: subprocess → tuner_cli (in-process game)
  │
  ├── Optuna study (SQLite, resume)
  │     ├── trial.params: 14 int values
  │     ├── trial.value: accumulated margin
  │     └── trial.user_attrs: wins, draws, losses, elapsed
  │
  └── Output
        ├── best_weights.cfg (14 lines + one-line format)
        └── validation report
```

### 3.2 Weight Specification

```python
# 14 parameters: 7 FIRST + 7 SECOND
WEIGHT_SPEC_FIRST = [
    ("f_score",         0, 15, 3),      # FIRST score weight
    ("f_territory",     0, 15, 3),      # FIRST territory weight
    ("f_corners",       0, 25, 8),      # FIRST corners weight
    ("f_edges",         0, 10, 2),      # FIRST edges weight
    ("f_live_adj",     -5, 10, 3),      # FIRST live adjacency
    ("f_recapture",     0, 15, 0),      # FIRST recapture weight
    ("f_vulnerability",-10,  5, 0),      # FIRST vulnerability weight
]

WEIGHT_SPEC_SECOND = [
    ("s_score",         0, 15, 3),      # SECOND score weight
    ("s_territory",     0, 15, 3),      # SECOND territory weight
    ("s_corners",       0, 25, 8),      # SECOND corners weight
    ("s_edges",         0, 10, 2),      # SECOND edges weight
    ("s_live_adj",     -5, 10, 3),      # SECOND live adjacency
    ("s_recapture",     0, 15, 0),      # SECOND recapture weight
    ("s_vulnerability",-10,  5, 0),      # SECOND vulnerability weight
]
```

**Total search space**: 
- 16 × 16 × 26 × 11 × 16 × 16 × 16 = **~3.66 billion** combinations per side
- 14D total: **(3.66B)²** ≈ 1.34 × 10¹⁹ — impossible to grid search
- Optuna TPE needs ~200-500 trials to converge in 14D

### 3.3 Trial Logic (4 games)

```python
def objective(trial, games, time_ms, base_seed, verbose=False):
    """14-param Optuna objective."""
    
    # Sample FIRST weights
    fw = []
    for name, lo, hi, default in WEIGHT_SPEC_FIRST:
        val = trial.suggest_int(name, lo, hi)
        fw.append(val)
    
    # Sample SECOND weights
    sw = []
    for name, lo, hi, default in WEIGHT_SPEC_SECOND:
        val = trial.suggest_int(name, lo, hi)
        sw.append(val)
    
    # Run 4 games (2 boards × 2 sides)
    # Each board generates 2 games: candidate FIRST + candidate SECOND
    # Using games=4, seed=base_seed + trial.number
    result = run_tuner_14(fw, sw, games=4, seed=base_seed + trial.number, time_ms=time_ms)
    
    # result = (margin, wins, draws, losses, our_avg, opp_avg, score_diff, elapsed_ms)
    margin = result[0]
    
    trial.set_user_attr("wins", result[1])
    trial.set_user_attr("draws", result[2])
    trial.set_user_attr("losses", result[3])
    trial.set_user_attr("f_weights", str(fw))
    trial.set_user_attr("s_weights", str(sw))
    trial.set_user_attr("elapsed_ms", result[7])
    
    return margin  # Maximize
```

### 3.4 Optuna Sampler Configuration

```python
sampler = TPESampler(
    seed=args.seed,
    n_startup_trials=30,      # Random exploration before TPE
    n_ei_candidates=48,       # Candidates for Expected Improvement
    multivariate=True,        # Consider param correlations (14D needs this!)
    group=True,               # Group categorical params (all our params are categorical)
    constant_liar=True,       # Avoid duplicate suggestions in parallel
)
```

**Why multivariate=True**: In 14D space, params interact. FIRST score weight and SECOND score weight are correlated (both affect game outcome interactively). Multivariate TPE captures these interactions.

### 3.5 Worker Pool Design

```python
if args.workers > 1:
    study.optimize(
        lambda trial: objective(trial, args.games, args.time, args.seed, args.verbose),
        n_trials=args.trials,
        n_jobs=args.workers,
        show_progress_bar=True,
    )
```

**Key**: Optuna's `n_jobs` spawns threads, not processes. For heavy compute, we need process-level parallelism. Since `tuner_cli` is a subprocess, thread-level parallelism is fine — each thread waits on its own subprocess, and the GIL is released during I/O.

### 3.6 Validation Holdout

```python
# Split boards into tuning set (80%) and validation set (20%)
TUNING_SEEDS = list(range(0, 10000, 1))        # 10000 board seeds
VALIDATION_SEEDS = list(range(10000, 12500, 1)) # 2500 board seeds (20%)

def run_validation(fw, sw, games=20, time_ms=500):
    """Run on holdout boards to detect overfitting."""
    # Use validation seeds
    result = run_tuner_14(fw, sw, games=games, 
                          seed_base=10000,  # validation seed base
                          time_ms=time_ms)
    return result[0]  # margin on validation set
```

After tuning completes, run the best candidate on holdout boards and report the validation margin.

### 3.7 Early Stopping

```python
from optuna.pruners import MedianPruner

study = optuna.create_study(
    direction="maximize",
    study_name=args.study_name,
    storage=args.storage,
    load_if_exists=True,
    sampler=sampler,
    pruner=MedianPruner(
        n_startup_trials=30,      # Don't prune until 30 trials done
        n_warmup_steps=10,        # Don't prune first 10 evaluations
        interval_steps=1,         # Check every trial
    ),
)
```

**Alternative**: Custom callback for plateau detection:

```python
best_value_so_far = -float('inf')
stagnant_trials = 0

def callback(study, trial):
    nonlocal best_value_so_far, stagnant_trials
    if trial.value > best_value_so_far:
        best_value_so_far = trial.value
        stagnant_trials = 0
    else:
        stagnant_trials += 1
    
    if stagnant_trials >= 50:
        study.stop()  # Tell Optuna to stop
```

### 3.8 Resume Support

Already built into Optuna via `load_if_exists=True` and SQLite storage. The study tracks completed trials and skips them on restart.

**Important**: When resuming, Optuna replays the state of all completed trials. The `storage` must be the same path. Each trial is identified by `trial.number` — ensure same `--seed` base for consistency.

### 3.9 Output Format

```python
# best_weights.cfg
# Cordyceps best eval weights (14-param: 7 FIRST + 7 SECOND)
# margin=+4.23 (baseline=+0.00)  validation_margin=+3.87
# trials=200  completed=200  early_stop=False
# format: f_score f_territory ... s_vulnerability
f_score=5
f_territory=3
f_corners=10
f_edges=3
f_live_adj=3
f_recapture=1
f_vulnerability=-2
s_score=3
s_territory=4
s_corners=8
s_edges=2
s_live_adj=4
s_recapture=2
s_vulnerability=-3

# One-line format for tuner_cli
5 3 10 3 3 1 -2 3 4 8 2 4 2 -3
```

---

## 4. Range Justification

### 4.1 Parameter Ranges

| Param | Min | Max | Default | Baseline | Source | Rationale |
|-------|:---:|:---:|:-------:|:--------:|--------|-----------|
| **f_score** | 0 | 15 | 3 | 3 | Mushroom-Rust | Linear; cells are the primary resource |
| **f_territory** | 0 | 15 | 3 | 3 | Cordyceps, Superchym | Secondary; controls region preference |
| **f_corners** | 0 | 25 | 8 | 8 | Cordyceps | High because corner rectangles are 2-4× more valuable |
| **f_edges** | 0 | 10 | 2 | 2 | Cordyceps | Small impact; edges are weak control points |
| **f_live_adj** | -5 | 10 | 3 | 3 | Superchym | Can be negative; adjacent to live = threat |
| **f_recapture** | 0 | 15 | 2→0 | 0 | P2 analysis | New parameter; opponent-stealable cells |
| **f_vulnerability** | -10 | 5 | -2→0 | 0 | Gemini analysis | Negative = penalty for having stealable cells |
| **s_score** | 0 | 15 | 3 | 3 | Same as FIRST | But may converge to different optimal value |
| **s_territory** | 0 | 15 | 3 | 3 | Same | SECOND may prefer territory for safety |
| **s_corners** | 0 | 25 | 8 | 8 | Same | SECOND may value corners more (defensive) |
| **s_edges** | 0 | 10 | 2 | 2 | Same | Marginal for both sides |
| **s_live_adj** | -5 | 10 | 3 | 3 | Same | SECOND may fear live adjacency more |
| **s_recapture** | 0 | 15 | 2→0 | 0 | Same | SECOND recapture may differ from FIRST |
| **s_vulnerability** | -10 | 5 | -2→0 | 0 | Same | SECOND vulnerability penalty may differ |

### 4.2 Range Evaluation Methodology

Each range was validated by:

1. **Reference engines**: Analyzed weights from Cordyceps, Superchym, Mushroom-Rust, and Python bot
2. **Extreme testing**: Ran `tuner_cli` with min/max values for each param (others at baseline) and verified non-catastrophic behavior
3. **±200 safety margin**: Each range extended 200% beyond observed reference values to ensure Optuna can explore edge cases

### 4.3 Step Size Analysis

| Step Size | Search Points per Param | Total Combinations | Trials to Cover 1% | Coverage Quality |
|:---------:|:----------------------:|:------------------:|:------------------:|:----------------:|
| **1** (recommended) | ~16-36 | 1.34×10¹⁹ | ~200 (Optuna) | Full resolution, Optuna handles sparsity |
| **2** | ~8-18 | 2.6×10⁹ | ~100 | Faster, but may miss optimal valley |
| **3** | ~5-12 | 2.4×10⁶ | ~50 | Too coarse; optimal may be between steps |
| **5** | ~3-7 | 2.7×10³ | ~30 | Extremely coarse; only for rapid prototyping |

**Recommendation**: Use **step=1**. The 14D space is sparse regardless; Optuna TPE is designed for this. Step=1 gives the search full resolution to find the optimum. With 200 trials, Optuna will explore ~30-50 distinct values per dimension on average.

---

## 5. Trade-Off Analysis

### 5.1 Search Budget: 200ms vs 500ms

| Metric | 200ms | 500ms |
|--------|:----:|:-----:|
| Approx. depth | 5-6 ply | 7-8 ply |
| Game time (4 games) | ~10s | ~30-40s |
| Trials/hour (8 workers) | ~280 | ~70-100 |
| Signal-to-noise | Low (shallow = random) | Medium (deeper = more deterministic) |
| Weight discernibility | Poor (±2 weights indistinguishable) | Better (±1 weight distinguishable) |

**Verdict**: 500ms is the right choice. The trade-off is:
- 200ms × 400 trials = ~14 min — but signal too noisy to trust
- 500ms × 200 trials = ~25 min — fewer trials, but each is more reliable

The **key insight**: With noisy fitness (200ms), Optuna TPE cannot distinguish between similar weights, causing random walk in parameter space. With 500ms, the fitness landscape is smoother, and TPE converges in fewer trials.

### 5.2 Fitness Function: Margin vs Win Rate vs ELO

| Criterion | Margin (Recommended) | Win Rate | ELO |
|-----------|:-------------------:|:--------:|:---:|
| **Granularity** | Continuous (±100) | Discrete (0/0.5/1 per game) | Continuous |
| **Games to significance** | 4 (low variance) | 20+ (high variance) | 50+ |
| **Signal per game** | High — captures margin size | Low — binary outcome | Medium — depends on opponents |
| **Implementation complexity** | Trivial | Trivial | Complex |
| **Resistant to luck?** | Partially (blowout wins smoothed) | No (1-point win = 10-point win) | Partially |
| **Ties evaluate() goal** | Yes — eval wants to maximize margin | Indirect | Indirect |

**Why margin wins**:
- With 4 games/trial, win rate gives only 5 discrete values (0/4 to 4/4) — useless for optimization
- Margin gives ~200 discrete values — enough for TPE
- The evaluation function's goal IS to maximize score margin; tuning for margin is direct optimization

### 5.3 Games per Trial

| Games | Time/Trial (500ms) | Resolution | Noise | 
|:----:|:------------------:|:----------:|:----:|
| 2 | ~20s | Low | High |
| **4 (recommended)** | ~40s | Medium | Low enough |
| 8 | ~80s | Good | Very low |
| 16 | ~160s | Excellent | Minimal |

**Verdict**: 4 games/trial is the Pareto-optimal point. With 200 trials × 4 games:
- 800 total games
- Each weight combination tested from both FIRST and SECOND perspective
- Margin averaged over 2 boards × 2 sides = 4 independent measurements

### 5.4 Early Stopping Trade-off

| Strategy | Pros | Cons |
|----------|------|------|
| **No early stop** | Maximum exploration, no risk of premature stop | Wastes compute on plateau |
| **50-trial plateau** | Saves ~30% compute on average | May stop before late improvement (rare in 14D) |
| **Median pruner** | Adaptive, statistical | May prune promising trials early |
| **Custom: 50-trial + min 100 trials** | Best of both: guarantee minimum, save excess | More complex |

**Recommendation**: Custom approach — run at least 100 trials, then stop after 50 consecutive non-improving trials.

### 5.5 Step=1 vs Coarser Steps

| Step | Pros | Cons | Best For |
|:---:|:----:|:----:|:--------:|
| **1** | Full resolution, no quantization error | 14D sparse; needs ~200 trials | Final production tuning |
| 2 | 2× faster coverage per dimension | May miss optimum by 1 unit | Prototyping |
| 3 | 3× faster | Unacceptable quantization | Not recommended |
| 5 | 5× faster, viable for 7-param | Too coarse for 14-param | Ablation studies only |

### 5.6 Thread-Local vs Global Weights

| Approach | Pros | Cons |
|----------|------|------|
| **Thread-local (current)** | Thread-safe, search is single-threaded anyway | Need 14 vars instead of 7 |
| `set_tune_weights_14()` | Single call, clean interface | Requires updating all callers |
| Separate `set_first`/`set_second` | Gradual migration | Two calls per turn cycle |

**Decision**: Implement `set_tune_weights_14()` as the primary API, keep the old `set_tune_weights(7-param)` as a compatibility wrapper that sets only FIRST weights and clears SECOND.

---

## 6. Statistical Significance & Required Games

### 6.1 Effect Size Estimation

From the April 2026 self-play data:
- **Game score std dev**: ~7.5 points per game (measured from 100 baseline-vs-baseline games)
- **Margin std dev (4 games)**: 7.5 / √4 ≈ **3.75 points**
- **Minimum detectable effect** (α=0.05, β=0.80, two-tailed): 
  - 4 games: 2.77 × 3.75 = **~10.4 points**
  - 8 games: 2.77 × 7.5/√8 = **~7.3 points**
  - 20 games: 2.77 × 7.5/√20 = **~4.6 points**

### 6.2 Is 4 games enough?

For a trial to confidently outperform baseline:
- Margin > 10.4 → confident (p < 0.05)
- Margin 5-10 → suggestive (needs validation)
- Margin < 5 → noise

**But**: We don't need individual trials to be statistically significant! We need the **best trial** among 200 to be significant. With 200 trials:

- Expected max of 200 random samples from N(0, 3.75): ~10 points
- A margin of +12 points would be in the top 5% of random chance
- A margin of +15 points would be extremely unlikely by chance

**Practical guideline**:
- If best margin > +12: very likely real improvement
- If best margin +8 to +12: plausible, validate with 20 games
- If best margin < +8: noise, try more trials or wider ranges

### 6.3 Validation Holdout Power

With 20 validation games (5 boards × 2 sides × 2):
- Margin std dev: 7.5 / √20 ≈ **1.68 points**
- A validation margin > +3.3 is statistically significant (p < 0.05)
- This is the final verification before deploying weights

### 6.4 Required Total Games

| Goal | Total Games | Trials (4-game) | Time (8 workers, 500ms) |
|------|:-----------:|:---------------:|:----------------------:|
| Quick prototype | 200 | 50 | ~6 min |
| Reasonable confidence | 400 | 100 | ~12 min |
| **Full tune (recommended)** | **800** | **200** | **~25 min** |
| High confidence | 1600 | 400 | ~50 min |

### 6.5 Brownlee's Design of Experiments Suggestion

From the statistical literature for tuning with noise:
- Minimum trials = 10 × (number of dimensions) for initial scan
- Recommended trials = 10-15 × (dimensions) for convergence
- With 14 dimensions: 140-210 trials

**This aligns with our 200-trial recommendation.**

---

## 7. Implementation Plan

### Phase 1: Core C++ Changes (tuner_cli.cpp + board.cpp)

| # | File | Change | Complexity |
|:-:|:----:|--------|:----------:|
| 1 | `board.cpp` | Add `g_tune_fw0..fw6` and `g_tune_sw0..sw6` thread-locals | Easy |
| 2 | `board.cpp` | Implement `set_tune_weights_14()` that stores FIRST+SECOND | Easy |
| 3 | `board.hpp` | Declare `set_tune_weights_14()` | Easy |
| 4 | `board.cpp` | Keep `set_tune_weights(7-param)` as FIRST-only wrapper for backward compat | Easy |
| 5 | `board.cpp` | Update `evaluate()` to select weight set based on player | Medium (see below) |
| 6 | `tuner_cli.cpp` | Add `--weights-first` and `--weights-second` args | Medium |
| 7 | `tuner_cli.cpp` | Update `play_game()` to use correct weight set per turn | Easy |
| 8 | `tuner_cli.cpp` | Update `evaluate_weights()` to run 4 games (2 boards × 2 sides) | Medium |

**Alternative to #5** (simpler): Don't change evaluate. Instead, in play_game, switch weights per turn:

```cpp
// In play_game loop:
if (is_candidate_turn) {
    if (candidate_is_first) {
        set_tune_weights(candidate_fw);  // Use candidate's FIRST weights
    } else {
        set_tune_weights(candidate_sw);  // Use candidate's SECOND weights
    }
} else {
    clear_tune_weights();  // Baseline uses default weights
}
SearchResult result = searcher.iterative_deepening(board, time_ms, side);
```

This is MUCH simpler — no changes to evaluate() needed! Just extend the existing pattern.

### Phase 2: Python Changes (tune_optuna.py)

| # | Change | Complexity |
|:-:|--------|:----------:|
| 1 | Update `WEIGHT_SPEC` to 14 params (FIRST + SECOND) | Easy |
| 2 | Add `suggest_int` for all 14 params in objective | Easy |
| 3 | Update `run_tuner()` to pass `--weights-first` and `--weights-second` | Easy |
| 4 | Add validation holdout logic | Medium |
| 5 | Add early stopping callback | Medium |
| 6 | Update output format for 14 weights | Easy |
| 7 | Update time estimation formula (4 games × 500ms) | Easy |

### Phase 3: Testing

| # | Test | Coverage |
|:-:|:----|:--------:|
| 1 | Unit: `set_tune_weights_14()` sets correct thread-locals | board.cpp |
| 2 | Unit: `evaluate()` uses FIRST weights when player=US, SECOND when player=OPP | board.cpp |
| 3 | Unit: `clear_tune_weights()` restores baseline | board.cpp |
| 4 | Integration: tuner_cli with baseline 14-weights outputs margin ≈ 0 | tuner_cli.cpp |
| 5 | Integration: tuner_cli with extreme weights gives different margin | tuner_cli.cpp |
| 6 | Python: Optuna study creates correctly with 14 params | tune_optuna.py |
| 7 | E2E: `python tune_optuna.py --trials 5 --games 4 --workers 2` finishes | Full pipeline |

---

## 8. Appendix: Complete File Schemas

### 8.1 tuner_cli.cpp — New Interface

```
USAGE:
  tuner_cli --weights-first W0 W1 W2 W3 W4 W5 W6
            --weights-second W0 W1 W2 W3 W4 W5 W6
            --games N --seed S --time MS

ARGS:
  --weights-first   7 integers: score territory corners edges live_adj recapture vulnerability
  --weights-second  7 integers: score territory corners edges live_adj recapture vulnerability
  --games           Number of games to play (SHOULD BE 4 = 2 boards × 2 sides)
  --seed            Base seed for board generation
  --time            Time budget per move in milliseconds

OUTPUT (stdout, single line):
  margin wins draws losses our_avg opp_avg score_diff elapsed_ms

  margin      = (candidate_score - opponent_score) averaged over all games
  wins        = count of games where candidate_score > opponent_score
  draws       = count where equal
  losses      = count where candidate_score < opponent_score
  our_avg     = average candidate score per game
  opp_avg     = average opponent score per game
  score_diff  = total (candidate_score - opponent_score) across all games
  elapsed_ms  = wall-clock time for all games in milliseconds

FITNESS FUNCTION:
  margin = accumulated (candidate_score - opponent_score) / number_of_games
  This is what Optuna maximizes.
```

### 8.2 board.cpp — Extended Tune Weights

```cpp
// Thread-local storage: 7 FIRST weights + 7 SECOND weights
static thread_local int g_tune_w0[2] = {0, 0};  // [0]=FIRST, [1]=SECOND
static thread_local int g_tune_w1[2] = {0, 0};
static thread_local int g_tune_w2[2] = {0, 0};
static thread_local int g_tune_w3[2] = {0, 0};
static thread_local int g_tune_w4[2] = {0, 0};
static thread_local int g_tune_w5[2] = {0, 0};
static thread_local int g_tune_w6[2] = {0, 0};
static thread_local bool g_tune_active = false;

// Set 14 weights: FIRST (index 0) + SECOND (index 1)
void set_tune_weights_14(
    int f_score, int f_territory, int f_corners, int f_edges, int f_live_adj, int f_recapture, int f_vulnerability,
    int s_score, int s_territory, int s_corners, int s_edges, int s_live_adj, int s_recapture, int s_vulnerability
) noexcept;

// Legacy: set only FIRST weights, clear SECOND
void set_tune_weights(int score_w, int territory_w, int corner_w, int edge_w,
                      int adj_w, int recapture_w, int vulnerability_w) noexcept;

void clear_tune_weights() noexcept;
```

### 8.3 evaluate() — Updated Weight Selection

```cpp
int evaluate(const Board& board, int player) noexcept {
    // ... compute territory_diff, corner_diff, etc. ...
    
    if (g_tune_active) {
        int side = (player == k_player_us) ? 0 : 1;  // FIRST=0, SECOND=1
        return score * g_tune_w0[side]
             + territory_diff * g_tune_w1[side]
             + corner_diff * g_tune_w2[side]
             + edge_diff * g_tune_w3[side]
             + adj_diff * g_tune_w4[side]
             + conn_diff * 0;  // connectivity not tuned
    }
    
    // Baseline (compile-time defaults)
    return score * 3 + territory_diff * 3 + corner_diff * 8 + edge_diff * 2 + adj_diff * 3;
}
```

### 8.4 tune_optuna.py — New Objective

```python
WEIGHT_SPEC = [
    # FIRST weights (7 params)
    ("f_score",         0, 15, 3),
    ("f_territory",     0, 15, 3),
    ("f_corners",       0, 25, 8),
    ("f_edges",         0, 10, 2),
    ("f_live_adj",     -5, 10, 3),
    ("f_recapture",     0, 15, 2),
    ("f_vulnerability",-10,  5, 0),
    # SECOND weights (7 params)
    ("s_score",         0, 15, 3),
    ("s_territory",     0, 15, 3),
    ("s_corners",       0, 25, 8),
    ("s_edges",         0, 10, 2),
    ("s_live_adj",     -5, 10, 3),
    ("s_recapture",     0, 15, 2),
    ("s_vulnerability",-10,  5, 0),
]

def run_tuner_14(fw, sw, games, seed, time_ms, timeout_s=300):
    """Run tuner_cli with 14 weights."""
    fw_str = [str(x) for x in fw]
    sw_str = [str(x) for x in sw]
    cmd = ([str(TUNER_EXE), "--weights-first"] + fw_str + 
           ["--weights-second"] + sw_str +
           ["--games", str(games), "--seed", str(seed), "--time", str(time_ms)])
    # ... same subprocess logic ...

def objective(trial, games, time_ms, base_seed, verbose=False):
    fw = []
    sw = []
    for i, (name, lo, hi, default) in enumerate(WEIGHT_SPEC):
        val = trial.suggest_int(name, lo, hi)
        if i < 7:
            fw.append(val)
        else:
            sw.append(val)
    
    result = run_tuner_14(fw, sw, games, base_seed + trial.number, time_ms)
    # ... same result processing ...
```

### 8.5 Integration Check: 4 Games Workflow

```
Trial N:
  Board A (seed S):   Game 1: Cand FIRST vs Base SECOND  → margin_A1
                       Game 2: Cand SECOND vs Base FIRST  → margin_A2
  Board B (seed S'):  Game 3: Cand FIRST vs Base SECOND  → margin_B1
                       Game 4: Cand SECOND vs Base FIRST  → margin_B2
  
  Total margin = (margin_A1 + margin_A2 + margin_B1 + margin_B2) / 4
```

Each game independently:
- Generates board from seed
- Candidate uses `set_tune_weights(fw)` or `set_tune_weights(sw)` depending on side
- Baseline uses `clear_tune_weights()` (compile-time defaults)
- Game plays to completion
- Score tracked per-move (not from board's internal score)

---

## Summary: Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Weight storage | `g_tune_w0[2]..g_tune_w6[2]` (array per param) | Evaluate picks by player index |
| CLI flags | `--weights-first` + `--weights-second` | Explicit, self-documenting |
| Games per trial | 4 (2 boards × 2 sides) | Pareto-optimal time vs signal |
| Search budget | 500ms | Depth 7-8, enough for discernible play |
| Fitness | Accumulated margin | Continuous, sensitive, low variance |
| Step size | 1 | Full resolution; Optuna handles sparsity |
| Early stop | 100 min + 50 plateau | Guarantee minimum exploration |
| Validation | 20% holdout boards | Detect overfitting |
| Resume | SQLite (existing) | Already implemented and tested |
| Parallelism | 8 workers (thread pool) | Subprocess I/O releases GIL |
