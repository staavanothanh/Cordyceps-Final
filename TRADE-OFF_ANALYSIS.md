# TRADE-OFF ANALYSIS: Eval Weight Tuning cho Cordyceps Engine

> **Game**: Mushroom Game (NYPC Master Track)  
> **Engine**: Cordyceps C++20 — Negamax α-β + TT + Iterative Deepening  
> **Tuner**: In-process `tuner_cli.cpp` + Optuna TPE (`tune_optuna.py`)  
> **Data sources**: Tournament logs, benchmark output, DESIGN_TUNER_14PARAM.md, PLAN.md  

---

## Mục Lục

1. [Hiện Trạng & Baseline Metrics](#1-hiện-trạng--baseline-metrics)
2. [Trade-Off 1: Self-play vs Opponent-based Tuning](#2-trade-off-1-self-play-vs-opponent-based-tuning)
3. [Trade-Off 2: Search Budget cho Tuning](#3-trade-off-2-search-budget-cho-tuning)
4. [Trade-Off 3: Range Precision (Step Size)](#4-trade-off-3-range-precision-step-size)
5. [Trade-Off 4: Fitness Function](#5-trade-off-4-fitness-function)
6. [Trade-Off 5: FIRST vs SECOND Weights](#6-trade-off-5-first-vs-second-weights)
7. [Trade-Off 6: Games/Trial & Trials Cần Thiết](#7-trade-off-6-gamestrial--trials-cần-thiết)
8. [Tổng Hợp & Kế Hoạch Tuning Theo Phases](#8-tổng-hợp--kế-hoạch-tuning-theo-phases)
9. [Implementation Roadmap](#9-implementation-roadmap)

---

## 1. Hiện Trạng & Baseline Metrics

### 1.1 Engine Performance Snapshot

| Metric | Giá trị | Nguồn |
|--------|---------|-------|
| Depth @ 200ms | ~7.1 ply | benchmark (Phase 3) |
| Depth @ 500ms | ~8.2 ply | benchmark (Phase 3) |
| TT hit rate @ depth 4+ | 55.2% | 10 samples @200ms |
| Nodes @ 200ms | ~24K | benchmark |
| Nodes @ 500ms | ~61K | benchmark |
| **vs agent + superchym (FIRST)** | **100%** (14/14) | Tournament Phase 5C |
| **vs agent + superchym (SECOND)** | **64%** (9/14) | Tournament Phase 5C |
| **TOTAL vs ref engines** | **82%** (23W 4L 1D) | 28 games post-fix |
| Baseline margin (7-param default) | +0.75 | `tune_optuna.py` baseline |
| Best tuned margin (7-param) | +3.60 | `best_weights.cfg` |

### 1.2 Current Tuner Architecture

```
tune_optuna.py
  └─ Optuna TPE sampler (n_startup_trials=30)
       └─ subprocess → tuner_cli --weights w0..w6 --games 20 --seed S --time 200
            └─ in-process play_game():
                 ├─ set_tune_weights(candidate_weights)
                 ├─ search.iterative_deepening(board, 200ms, side)  ← CANDIDATE
                 ├─ clear_tune_weights()
                 ├─ search.iterative_deepening(board, 200ms, side)  ← BASELINE (compile-time)
                 └─ accumulate margin
```

### 1.3 Current Evaluate Function (board.cpp)

```cpp
// Baseline (compile-time):
return score * 3 + territory_diff * 3 + corner_diff * 8 
     + edge_diff * 2 + adj_diff * 3 + conn_diff * 0;

// Tuned (best_weights.cfg):
// score=15, territory=13, corners=16, edges=8, live_adj=1, recapture=13, vulnerability=-3
// margin=+3.60 (baseline=+0.75)
```

### 1.4 Key Constraints

| Constraint | Giá trị | Impact on Tuning |
|-----------|---------|------------------|
| Time/turn (actual) | ~200-500ms | Budget cho search depth |
| Total time/game | ~10,000ms | ~15-30 moves/player |
| Game time @200ms | ~6s | 800 games = 1.3h |
| Game time @500ms | ~15s | 800 games = 3.3h |
| CPU cores | 1 (AWS c7a.2xlarge) | Search single-threaded |
| Tuner workers | 8 (thread pool) | Subprocess GIL-released |

---

## 2. Trade-Off 1: Self-play vs Opponent-based Tuning

### 2.1 Phân Tích

| Aspect | Self-play (current) | Opponent-based | AlphaZero-style |
|--------|-------------------|----------------|-----------------|
| **Đối thủ** | Baseline weights (compile-time) | agent.exe, superchym.exe | Self-play MCTS |
| **Overfit risk** | Thấp — baseline là uniform | **Cao** — chỉ 2 engines | Thấp — càng chơi càng general |
| **Tín hiệu** | Margin vs uniform weights | Win rate vs specific engines | Self-play improvement |
| **Compute cost** | 1 engine instance | 2 engine instances (subprocess) | 1 engine + MCTS |
| **AlphaZero parallel** | ✗ | ✗ | ✅ MCTS tự nhiên parallel |

### 2.2 Dữ Liệu Thực Tế

**Từ tournament phase 5C** (28 games post-fix):
```
vs agent:      79% (11W 2L 1D)  — score_diff = +30
vs superchym:  86% (12W 2L 0D)  — score_diff = +52

As FIRST:  100% (14/14)  — score_diff = +59
As SECOND:  64% (9/14)   — score_diff = +23
```

**Self-play noise measurement** (từ DESIGN_TUNER_14PARAM.md):
```
Score std dev (100 baseline-vs-baseline games): ~7.5 points
Margin std dev (4 games): 7.5 / √4 ≈ 3.75 points
```

### 2.3 Trade-Off: Self-play vs Opponent-based

#### Self-play (Recommended)

**Pros**:
- Không overfit vào specific opponent patterns
- Baseline weights là "trung tính" — không bias
- Chỉ cần 1 engine instance (tuner_cli in-process)
- Tương thích với Optuna TPE — nhiều trial hơn trong cùng time budget
- Nếu best weights thắng baseline margin, tournament win rate tự động tăng

**Cons**:
- Baseline weights yếu → tín hiệu nhiễu (shallow search + weak opponent = high variance)
- Self-play có thể reinforce eval biases (eval sai → search chọn move sai → reinforce)
- Không tối ưu hóa trực tiếp cho tournament opponents

**Baseline weakness analysis**:
```
Baseline weights: score*3, territory*3, corners*8, edges*2, live_adj*3
Best tuned:       score=15, territory=13, corners=16, edges=8, live_adj=1

→ Baseline ratio: score:territory:corners ≈ 1:1:2.7
→ Best ratio:     score:territory:corners ≈ 1:0.87:1.07
→ Baseline đánh giá corners quá cao so với score
→ Best tuned tăng score weight lên gấp 5× baseline
```

**Kết luận**: Self-play vẫn có giá trị vì baseline đủ yếu để tạo differentiable margin, nhưng...

#### Opponent-based

**Pros**:
- Tối ưu hóa trực tiếp tournament performance
- Tín hiệu gần với "real measure" hơn
- Có thể detect weight overfit qua cross-validation

**Cons**:
- Chỉ 2 opponents → overfit vào specific play style
- Cần chạy 2 engine instances + protocol I/O → chậm hơn 2-3×
- Optuna trial count giảm 50-66% trong cùng thời gian
- risk: weights thắng agent/superchym nhưng thua engine khác

#### AlphaZero-style (impractical here)

**Why not feasible**:
- MCTS không available (engine dùng Negamax α-β)
- Self-play với MCTS cần 1000+ simulations/move → depth không đủ
- Policy network không available
- Compute budget không đủ cho neural network inference

### 2.4 Verdict

```
RECOMMENDATION: SELF-PLAY (keep current approach)
──────────────────────────────────────────────
Rationale:
1. Self-play cho phép nhiều trial hơn trong cùng time budget
2. Overfit risk thấp hơn — generalize tốt hơn cho tournament
3. Baseline weak enough để tạo differentiable signal
4. Có thể cross-validate với opponents sau tuning

Mitigation for eval bias reinforcement:
  → Phase-based tuning: tune step=5 weights → validate vs opponents → refine
  → Use opponent validation as final gate, not training signal
```

---

## 3. Trade-Off 2: Search Budget cho Tuning

### 3.1 Depth vs Budget Relationship (từ benchmark)

| Budget | Avg Depth | Nodes | TT Hit Rate | Game Time (4 games) | Signal Reliability |
|:------:|:---------:|:-----:|:-----------:|:-------------------:|:------------------:|
| 200ms | 7.1 | 24K | 55.2% | ~6s | **Low** |
| 500ms | 8.2 | 61K | 54.5% | ~15s | **Medium** |
| 1000ms | ~9 | ~140K | ~54% | ~30s | **High** |

### 3.2 Trial Throughput Analysis (8 workers)

```
200ms:
  Per game:   ~6s (30 moves × 200ms)
  4 games:    ~24s
  Per trial:  ~25s (with overhead)
  Trials/h:   ~1,150
  200 trials: ~10 min

500ms:
  Per game:   ~15s (30 moves × 500ms)
  4 games:    ~60s  
  Per trial:  ~65s (with overhead)
  Trials/h:   ~440
  200 trials: ~27 min

1000ms:
  Per game:   ~30s (30 moves × 1000ms)
  4 games:    ~120s
  Per trial:  ~130s
  Trials/h:   ~220
  200 trials: ~54 min
```

### 3.3 Signal-to-Noise Analysis

**The critical question**: At what depth does weight differentiation become reliable?

From search theory:
- Depth 5-6 (200ms): eval noise dominates — search horizon error ±30%
  - Two weight sets that differ by 1-2 units per parameter → margin difference < 3 points (buried in noise)
  - TPE sees random correlation
- Depth 7-8 (500ms): eval signal detectable
  - Horizon effect reduced — deeper search means more reliable positional understanding
  - Weight differences of 2-3 units → margin difference 3-6 points (detectable)
- Depth 9+ (1000ms): diminishing returns
  - Additional depth gives marginal improvement in weight discernibility
  - But trial count halves

**Empirical evidence from current tuner**:
```python
# Current: 200ms, 20 games/trial
# Best margin found: +3.60 (baseline: +0.75)
# But tournament win rate: 82% 
# → The baseline margin of +0.75 is misleading
# → Actual improvement from tuning is smaller than measured

# 200ms overestimates margin because:
#   1. Baseline plays at depth 5-6 (poor positional play)
#   2. Candidate with any weight tweak that happens to work at shallow depth gets high margin
#   3. At tournament depth 8+, the advantage may not hold
```

### 3.4 The Goldilocks Budget

```
200ms:  Too noisy — TPE converges slowly, wastes trials on noise
500ms:  Just right — 8 workers × 440 trials/h, signal detectable
1000ms: Too slow — halves trial count, marginal signal improvement

RECOMMENDATION: 500ms
─────────────────
- Best trade-off between trial throughput and signal reliability
- Depth 7-8 is enough for meaningful positional evaluation
- 8 workers × 27 min for 200 trials = practical
```

### 3.5 Adaptive Budget Strategy

Instead of fixed budget, use adaptive budget based on phase:

| Phase | Budget | Rationale |
|-------|:------|-----------|
| Random exploration (trials 1-30) | 200ms | Cheap exploration, detect broad regions |
| TPE exploitation (trials 31-100) | 500ms | Converge to promising regions |
| Fine tuning (trials 101-200) | 500ms or 1000ms | Final precision — fewer trials needed |

**But**: This introduces uncontrolled variable (budget changes).
**Simpler**: Just use 500ms throughout. 27 min is acceptable.

### 3.6 Verdict

```
RECOMMENDATION: 500ms budget
─────────────────────────────
- Depth 7-8: reliable enough for weight differentiation
- 200 trials × 4 games = 27 min with 8 workers
- Signal-to-noise ratio acceptable for TPE convergence
- Adaptive budget adds complexity without clear benefit
```

---

## 4. Trade-Off 3: Range Precision (Step Size)

### 4.1 Parameter Ranges (7-param)

| Param | Min | Max | Range | Step=1 | Step=3 | Step=5 |
|-------|:---:|:---:|:-----:|:------:|:------:|:------:|
| score | 0 | 15 | 16 | 16 vals | 6 vals | 4 vals |
| territory | 0 | 15 | 16 | 16 vals | 6 vals | 4 vals |
| corners | 0 | 25 | 26 | 26 vals | 9 vals | 6 vals |
| edges | 0 | 10 | 11 | 11 vals | 4 vals | 3 vals |
| live_adj | -5 | 10 | 16 | 16 vals | 6 vals | 4 vals |
| recapture | 0 | 15 | 16 | 16 vals | 6 vals | 4 vals |
| vulnerability | -10 | 5 | 16 | 16 vals | 6 vals | 4 vals |

### 4.2 Search Space Size

```
7-param (single set):
  Step=1: 16×16×26×11×16×16×16 = ~3.0×10⁹ combinations
  Step=3: 6×6×9×4×6×6×6 = 279,936 combinations
  Step=5: 4×4×6×3×4×4×4 = 18,432 combinations

14-param (FIRST+SECOND):
  Step=1: (3.0×10⁹)² = 9.0×10¹⁸ combinations
  Step=3: (2.8×10⁵)² = 7.8×10¹⁰ combinations
  Step=5: (1.8×10⁴)² = 3.4×10⁸ combinations
```

### 4.3 What Can Each Method Cover?

| Method | Trials | 7-param Step=1 | 7-param Step=5 | 14-param Step=1 |
|--------|:-----:|:--------------:|:--------------:|:---------------:|
| Grid Search | — | Impossible (3B) | ✅ Feasible (18K) | Impossible (9e18) |
| Random Search | 200 | 0.000007% | 1.1% | 2.2e-15% |
| **Optuna TPE** | **200** | **~1%** effective coverage | **~10%** effective coverage | **~0.001%** |
| GA 50 gen × 100 pop | 5,000 | 0.00017% | 27% | 5.5e-14% |

**Effective coverage**: TPE focuses on promising regions — it doesn't need to cover the whole space.

### 4.4 Trade-Off: Step=1 vs Step=5

#### Step=1 (Recommended for TPE)

**Pros**:
- Full resolution — no quantization error
- TPE can find exact optimum
- Categorical params → TPE handles naturally (each int is a category)

**Cons**:
- Search space is 3B× larger than step=5
- TPE needs ~30-50 startup trials before it starts converging
- Risk: TPE may not converge fully in 200 trials for 14-param

**TPE startup cost**:
```python
# n_startup_trials = 30 (current setting)
# In 30 trials, TPE samples uniformly from space
# For 14-param: 30 samples in 9×10¹⁸ space = completely lost
# But TPE doesn't need to "cover" — it builds probabilistic models

# After startup, TPE uses Expected Improvement to sample promising regions
# In high-dimensional space, TPE convergence rate ≈ O(log n) per dimension
# 200 trials × log(200) ≈ 1060 effective evaluations per dimension
```

#### Step=5

**Pros**:
- Grid search + GA hybrid feasible for 7-param (18K combinations)
- Faster convergence — fewer trials needed
- Less noise in each dimension

**Cons**:
- Optimal weight may lie between step=5 values
- Quantization error: if optimum is at corners=13, step=5 gives 10 or 15
- For 14-param: 324M combinations — still can't grid search

**Quantization error analysis**:
```python
# If true optimal corner weight = 13:
# Step=1: 13 (exact)
# Step=5: 10 or 15 (±2-3 units)

# Impact on eval:
# Corner diff range: [-25, 25] cells
# Error at corners=10 vs 13: 3 × 8 (avg corner diff) = 24 eval units
# Error at corners=15 vs 13: 2 × 8 = 16 eval units
# This is significant enough to affect move selection!
```

### 4.5 Hybrid Strategy

The best approach combines both:

| Phase | Step | Method | Purpose |
|:-----:|:----:|:------:|---------|
| **Phase 1: Broad search** | 3 | TPE | Find promising regions (100 trials) |
| **Phase 2: Fine tuning** | 1 | TPE (warm-start) | Refine around Phase 1 best (100 trials) |
| **Phase 3: Validation** | — | Tournament | Validate best weights vs opponents |

**Warm-starting TPE**:
```python
# Phase 1: search with step=3
study1 = optuna.create_study(...)
study1.optimize(objective_step3, n_trials=100)

# Phase 2: use best params as seed for step=1
# Optuna doesn't support warm-start natively
# Workaround: constrain search ranges around Phase 1 best
best = study1.best_params
NARROW_RANGES = {
    "score": max(0, best["score"]-3), min(15, best["score"]+3),
    # ... narrow ±3 around best
}
```

### 4.6 Verdict

```
RECOMMENDATION: Step=1 with TPE (for final tuning)
                 Step=3 for rapid exploration
─────────────────────────────────────────────────
- TPE is designed for high-dimensional categorical search
- Step=1 avoids quantization error
- 200 trials with TPE is enough for 7-param convergence
- For 14-param: may need 300-500 trials
- Step=3 for initial exploration, Step=1 for final refinement
```

---

## 5. Trade-Off 4: Fitness Function

### 5.1 Candidates Compared

| Criterion | Margin (score diff) | Win Rate | Elo |
|-----------|:-------------------:|:--------:|:---:|
| **Type** | Continuous | Binary (per game) | Continuous |
| **Values per 4 games** | ~200 (range ±100) | 5 (0/4 to 4/4) | ~50 (depends on opponent) |
| **Std dev (4 games)** | ~3.75 | ~0.5 (binomial) | ~25-50 |
| **Games to significance** | 4 | 20+ | 50+ |
| **Granularity** | High | **Very low** | Medium |
| **Correlation w/ eval** | **Direct** | Indirect | Indirect |

### 5.2 Margin vs Win Rate: Measurement Resolution

```
4 games:
  Margin:  [-100, -99, ..., 0, ..., +99, +100]  ← 201 discrete values
  Win rate: [0%, 25%, 50%, 75%, 100%]           ← 5 discrete values

8 games:
  Margin:  [-200, -199, ..., +200]              ← 401 values  
  Win rate: [0%, 12.5%, 25%, ..., 100%]         ← 9 values

20 games:
  Margin:  [-500, ..., +500]                    ← 1001 values
  Win rate: [0%, 5%, ..., 100%]                 ← 21 values
```

**Critical insight**: With 4 games/trial, win rate has only 5 possible values. TPE cannot distinguish between:
- margin=+2 (close game, slightly better) vs margin=+20 (domination)
- Both give 75% win rate (3 wins, 1 loss)

**But margin sees the difference clearly**: +2 vs +20.

### 5.3 Margin Distribution From Real Data

From tournament logs (analyzed):
```
Score diffs observed in 28 games post-fix:
  FIRST: mean diff = +59/14 = +4.2 per game
  SECOND: mean diff = +23/14 = +1.6 per game
  
Typical margin distribution (from 100 baseline games):
  Mean: ~0 (balanced)
  Std:  ~7.5 per game
  95% CI: ±15 points
  
→ 4-game margin std = 7.5/√4 = 3.75
→ A margin of +10 is ~2.7σ from baseline → p < 0.01
```

### 5.4 The Problem with Win Rate

```
With 4 games/trial:

Observed win rate = 4/4 (100%)  → 1 combination
Observed win rate = 3/4 (75%)   → 4 combinations  
Observed win rate = 2/4 (50%)   → 6 combinations
Observed win rate = 1/4 (25%)   → 4 combinations
Observed win rate = 0/4 (0%)    → 1 combination

→ Only 16 possible outcomes
→ 4/4 and 3/4 both look like "good" but margin could be +2 or +20
→ TPE cannot distinguish between "barely winning" and "dominating"
→ Optuna will plateau early because fitness landscape is too flat
```

### 5.5 Margin Calibration

Margin has a known issue: **scale varies with opponent strength**.

```python
# Against baseline (weak): margin can be +30-50
# Against tuned (strong): margin can be +5-15

# Solution: always compare against the SAME baseline
# Use compile-time weights as fixed reference
# Margin = candidate_score - baseline_score
# This is what current tuner does ✓
```

### 5.6 Elo Alternative

Elo requires:
- Fixed pool of opponents
- 50+ games for ±50 Elo confidence
- Opponent strength must be known/estimated

**Not practical for tuning** — too expensive.

### 5.7 Validation Strategy

Use **both** margin and win rate in a split workflow:

```
┌──────────────────────┐
│  Tuning Phase         │
│  Fitness: Margin      │  ← 4 games/trial, continuous signal
│  Optuna maximizes     │
└──────────┬───────────┘
           ↓
┌──────────────────────┐
│  Validation Phase     │
│  Metric: Win Rate     │  ← 20+ games, binary outcome
│  vs opponents         │  ← real tournament conditions
└──────────┬───────────┘
           ↓
┌──────────────────────┐
│  Final Decision       │
│  Weights with best    │
│  tournament win rate  │
└──────────────────────┘
```

### 5.8 Verdict

```
RECOMMENDATION: Margin for fitness, Win Rate for validation
──────────────────────────────────────────────────────────
- Margin: 201 values per 4 games → smooth fitness landscape for TPE
- Win rate: 5 values per 4 games → useless for optimization
- Elo: impractical for tuning (needs 50+ games per evaluation)
- Validation: 20+ games vs opponents using win rate
```

---

## 6. Trade-Off 5: FIRST vs SECOND Weights

### 6.1 Empirical Evidence for Asymmetry

**From tournament Phase 5C** (post-bug-fix, 28 games):
```
As FIRST:  100% (14/14)  score_diff=+59  DOMINANT
As SECOND:  64% (9/14)   score_diff=+23  GOOD BUT WEAKER
```

**From log analysis**:
```
FIRST avg time: 150-250ms (balanced, confident)
SECOND avg time: 300-550ms (searching harder, playing catch-up)

FIRST move pattern: horizontal rects, spread-out territory
SECOND move pattern: vertical strips, aggressive steals
```

**Key insight**: FIRST và SECOND cần chiến lược khác nhau.
- FIRST: duy trì lợi thế, chơi an toàn, phòng thủ
- SECOND: cần lật ngược thế cờ, chấp nhận rủi ro

### 6.2 Current Limitation

**Current tuner**: 7 params, SAME weights for both sides.

```cpp
// Current: same weights for FIRST and SECOND
set_tune_weights(candidate_weights[0..6]);
// Both FIRST and SECOND use identical eval priorities
```

This is fundamentally limiting because:
- FIRST wants to emphasize **defense** (vulnerability penalty, territory holding)
- SECOND wants to emphasize **attack** (steal bonus, aggression)
- Different optimal weight ratios

### 6.3 Trade-Off: Joint vs Separate Tuning

#### Joint Tuning (14 params simultaneously)

**Pros**:
- Models interaction between FIRST and SECOND weights
- Finds global optimum (FIRST weights affect optimal SECOND weights and vice versa)
- Single tuning run

**Cons**:
- 14D search space — 3B× larger than 7D
- Slower convergence — needs 300-500 trials
- Correlation between FIRST/SECOND params makes TPE harder

**Interaction example**:
```python
# If FIRST overvalues territory (f_territory=12):
#   FIRST captures too much territory, spreads thin
#   SECOND can exploit: steal from spread-out FIRST
#   Optimal s_live_adj may be HIGH (aggressive steal)
# 
# If FIRST undervalues territory (f_territory=2):
#   FIRST doesn't defend territory
#   SECOND doesn't need to steal as much
#   Optimal s_recapture may be LOW
```

#### Separate Tuning

**Approach**:
1. Tune FIRST weights (7 params) with SECOND at baseline → best FIRST weights
2. Freeze FIRST at best, tune SECOND weights (7 params) → best SECOND weights
3. Optional: iterate once more

**Pros**:
- 7D space each — TPE converges in ~150 trials
- Clear ablation: see which weights improve which side
- Faster: 2 runs × 150 trials = 300 total (vs 400-500 for 14D)

**Cons**:
- Doesn't capture interaction between FIRST/SECOND
- May miss global optimum where both shift together
- Sequential: second tuning depends on first results

#### Impact Analysis

```
14D Joint:
  Space: 9.0×10¹⁸ combinations
  TPE convergence: ~300-500 trials
  Time: ~40-68 min @500ms 8 workers
  Quality: Potentially best (full interaction)

7D × 2 Separate:
  Space: 3.0×10⁹ each
  TPE convergence: ~150 trials each = 300 total
  Time: ~20 min each = 40 min total
  Quality: Good (misses cross-interaction)

Which is better?
────────────────
Depends on how strong the FIRST-SECOND interaction is.

Hypothesis: WEAK interaction.
  - FIRST and SECOND play fundamentally different roles
  - FIRST weight optimal value doesn't strongly depend on SECOND weights
  - Because search sees different positions for each side
  
Counter-hypothesis: MODERATE interaction.
  - If FIRST plays defensively (high vulnerability penalty),
    SECOND needs more aggression to overcome
  - FIRST weights affect what positions SECOND faces
```

### 6.4 Recommended Approach: Hybrid

```
Phase 1: Separate tuning (quick wins)
  Step 1: Tune FIRST (7 params) @ SECOND baseline → 150 trials
  Step 2: Freeze FIRST best, tune SECOND (7 params) → 150 trials
  Checkpoint: validate tournament win rate

Phase 2: Joint refinement (if needed)
  Step 3: Joint 14-param tuning around Phase 1 bests → 200 trials
  Step 4: Narrow ranges ±3 around Phase 2 best → 100 trials step=1
```

### 6.5 Implementation: 14-param evaluate()

The C++ change needed:

```cpp
// board.cpp — extend thread-local to 14 params (7 FIRST + 7 SECOND)
static thread_local int g_tune_fw[7] = {0};  // FIRST
static thread_local int g_tune_sw[7] = {0};  // SECOND  
static thread_local bool g_tune_active = false;

// Called once before search
void set_tune_weights_14(int fw[7], int sw[7]) noexcept {
    for (int i = 0; i < 7; ++i) {
        g_tune_fw[i] = fw[i];
        g_tune_sw[i] = sw[i];
    }
    g_tune_active = true;
}

// evaluate() picks based on player
int evaluate(const Board& board, int player) noexcept {
    // ...compute diffs...
    
    if (g_tune_active) {
        const int* w = (player == k_player_us) ? g_tune_fw : g_tune_sw;
        return score * w[0] + territory_diff * w[1] + corner_diff * w[2]
             + edge_diff * w[3] + adj_diff * w[4] + conn_diff * 0;
    }
    // baseline...
}
```

### 6.6 Verdict

```
RECOMMENDATION: HYBRID — Separate first, Joint if needed
─────────────────────────────────────────────────────────
Phase A: Tune FIRST alone (7 params) → 150 trials @500ms
Phase B: Tune SECOND alone (7 params) → 150 trials @500ms
Phase C: Optional joint refinement (14 params) → 200 trials @500ms
─────────────────────────────────────────────────────────
Time: ~40 min (A+B) or ~67 min (A+B+C)
Expected improvement:
  FIRST: already 100% — may not improve
  SECOND: 64% → target 70-80%
  Total: 82% → target 85-90%
```

---

## 7. Trade-Off 6: Games/Trial & Trials Cần Thiết

### 7.1 Variance Analysis

**From empirical data**:
```
Score std dev (per game, baseline vs baseline): σ ≈ 7.5 points

Margin std dev for N games:
  σ_N = 7.5 / √N

  4 games:  σ = 3.75
  8 games:  σ = 2.65
  16 games: σ = 1.88
  20 games: σ = 1.68 (current default!)
```

### 7.2 Statistical Power

**Minimum detectable effect** (MDE) at α=0.05, β=0.80:

| Games | MDE (points) | What it means |
|:-----:|:------------:|---------------|
| 4 | 10.4 | Can only detect LARGE improvements |
| 8 | 7.3 | Can detect moderate improvements |
| 16 | 5.2 | Can detect small improvements |
| 20 | 4.6 | Current setting — can detect small improvements |
| 40 | 3.3 | High precision but 2× compute |

**But**: We don't need each trial to be significant! We need the **best of 200 trials** to be significant.

```
Expected max of 200 random N(0, 3.75) samples: ≈ 10 points
Expected max of 200 random N(0, 2.65) samples: ≈ 7 points

→ 4 games: best of 200 needs margin > 12 to be "real" (p < 0.05)
→ 8 games: best of 200 needs margin > 9 to be "real"
```

### 7.3 Time vs Variance Trade-off

```
8 workers @500ms:

Games | Time/trial | 200 trials | σ_margin | MDE | Effective trials⁺
:----:|:----------:|:----------:|:--------:|:---:|:----------------:
  4   |    ~25s    |   ~14 min  |   3.75   | 10.4|       200
  8   |    ~50s    |   ~28 min  |   2.65   |  7.3|       400 (equivalent)
  16  |   ~100s    |   ~56 min  |   1.88   |  5.2|       800
  20  |   ~125s    |   ~70 min  |   1.68   |  4.6|      1000

⁺ Effective trials = trials × (σ_4/σ_N)² — adjusted for variance
```

### 7.4 Brownlee's Rule for 14 Parameters

From the DESIGN_TUNER_14PARAM.md:
```
Minimum trials = 10 × dimensions for initial scan
Recommended = 10-15 × dimensions for convergence
With 14 dims: 140-210 trials recommended
```

**But this assumes each trial gives reliable signal.** With noisy fitness:
```
Effective trials after noise adjustment:
  Actual trials × (1 / (1 + (σ/effect_size)²))

For typical effect size of 5 points:
  4 games (σ=3.75): effective = trials × 0.64
  8 games (σ=2.65): effective = trials × 0.78
  20 games (σ=1.68): effective = trials × 0.90
```

### 7.5 The Critical Number

For 14-param tuning, the trade-off is:

```
Option A: 4 games/trial × 400 trials = 1600 games → ~28 min
  Pros: Many trials, TPE explores more of search space
  Cons: Noisier signal, may converge to wrong region

Option B: 8 games/trial × 200 trials = 1600 games → ~28 min
  Pros: Cleaner signal, TPE converges more reliably
  Cons: Half the trials, less exploration
  
Option C: 4 games/trial × 200 trials = 800 games → ~14 min
  Pros: Fastest, practical for iteration
  Cons: Less reliable, may need multiple runs

Option D: 20 games/trial × 100 trials = 2000 games → ~70 min
  Pros: Very clean signal, high confidence
  Cons: Only 100 trials, TPE may not converge
```

**Key insight**: Total games matters more than trials or games/trial individually.

### 7.6 Recommended Configuration

```
PRIMARY RECOMMENDATION: 8 games × 200 trials = 1600 games, ~28 min
──────────────────────────────────────────────────────────────────
- 200 trials enough for 14D TPE convergence (Brownlee: 140-210)
- 8 games: σ=2.65, good signal-to-noise
- 28 min @8 workers — practical for iterative development
- 1600 games total — statistically meaningful

ALTERNATIVE (faster iteration): 4 games × 200 trials = 800 games, ~14 min
- Use for rapid prototyping / Phase 1 broad search
- Accept higher noise — results need confirmation
```

### 7.7 Validation Games

After finding best weights, validate with:
```
Validation:
  20 games vs baseline (self-play) → margin confidence ±3.3
  14 games vs agent (FIRST+SECOND) → win rate check
  14 games vs superchym (FIRST+SECOND) → win rate check
  Total: 48 games, ~12 min

Threshold for deployment:
  Validation margin > +3.3 (p < 0.05) against baseline
  Win rate vs opponents > current best (82%)
```

### 7.8 Verdict

```
RECOMMENDATION: 8 games/trial × 200 trials = 1600 games
───────────────────────────────────────────────────────
- Best balance of trial count and signal quality
- 28 min with 8 workers @500ms
- TPE has enough trials (200) for 14D convergence
- Equivalent to 400 trials of 4-game variance
```

---

## 8. Tổng Hợp & Kế Hoạch Tuning Theo Phases

### 8.1 Optimal Configuration Summary

| Trade-Off | Decision | Rationale |
|-----------|----------|-----------|
| **Self-play vs Opponent** | Self-play | Generalize better, more trials, no overfit |
| **Search budget** | 500ms | Depth 7-8, good signal, practical throughput |
| **Step size** | Step=1 (final), Step=3 (exploration) | Avoid quantization error; TPE handles 14D |
| **Fitness function** | Margin (tuning), Win rate (validation) | Margin: 200 values vs 5 for win rate |
| **FIRST vs SECOND** | Hybrid: separate → joint | Capture asymmetry, practical 7D each first |
| **Games/trial** | 8 games × 200 trials | σ=2.65, good signal, 200 trials for 14D |
| **Total compute** | ~28 min (8 workers, 500ms, 1600 games) | Practical for daily iteration |

### 8.2 Phase-Based Tuning Plan

```
PHASE 0: Baseline & Infrastructure (already done)
─────────────────────────────────────────────────
- [✅] tuner_cli.cpp working (7-param, 200ms, 20 games)
- [✅] tune_optuna.py working (TPE, SQLite resume, 8 workers)
- [✅] Tournament runner (testing_tool.py, tournament.py)
- [⚠️] Best weights found: score=15, territory=13, corners=16, 
       edges=8, live_adj=1, recapture=13, vulnerability=-3
       margin=+3.60 (baseline=+0.75)
- [⚠️] But tuned @200ms → signal unreliable

PHASE 1: 7-Param Refinement @500ms (1 session, ~14 min)
────────────────────────────────────────────────────────
Goal: Verify that 500ms tuning gives better weights than 200ms
Config: 7 params, 500ms, 4 games/trial, 200 trials
Output: Best 7-param weights (single set for both sides)
Validation: 20 games vs baseline + 28 games tournament

Expected: More reliable weights, better tournament performance
Risk: Low — infrastructure ready, just change --time 500

PHASE 2: Separate FIRST/SECOND Tuning (2 sessions, ~56 min)
────────────────────────────────────────────────────────────
Goal: Capture asymmetric play (FIRST defensive, SECOND aggressive)

Step 2A: Tune FIRST weights (7 params)
  - SECOND uses baseline (compile-time)
  - 500ms, 8 games/trial, 200 trials
  - Only FIRST-side games count toward margin
  - ~28 min

Step 2B: Tune SECOND weights (7 params)
  - Freeze FIRST at best from Step 2A
  - 500ms, 8 games/trial, 200 trials
  - Only SECOND-side games count toward margin
  - ~28 min

Output: best_first.cfg + best_second.cfg
Validation: 28 games tournament (14 FIRST + 14 SECOND)

Expected: SECOND improves from 64% → 70-80%
Risk: Medium — requires C++ change for 14-param evaluate()

PHASE 3: Joint 14-Param Refinement (optional, ~56 min)
───────────────────────────────────────────────────────
Goal: Capture interaction between FIRST/SECOND weights

Config: 14 params, 500ms, 8 games/trial, 200 trials
  - Narrow ranges ±3 around Phase 2 bests
  - ~28 min

If time permits: 100 more trials with step=1 for fine tuning
  - ~14 min

Output: best_14param.cfg
Validation: 28 games tournament (14 FIRST + 14 SECOND)

Expected: Marginal improvement over Phase 2
Risk: Low — narrow ranges minimize search space

PHASE 4: Tournament Validation (1 session, ~12 min)
────────────────────────────────────────────────────
Goal: Final verification before submission

1. 20 games vs baseline (self-play, 200ms tournament time)
2. 28 games vs agent + superchym (14 each, swap sides)
3. Compare: CURRENT vs PHASE1 vs PHASE2 vs PHASE3 weights

Decision: Submit best-performing weights

PHASE 5: Iterate (if time permits)
────────────────────────────────────
- Try 1000ms for critical weights (score, corners) 
- Try opponent-based tuning for specific weaknesses
- Try genetic algorithm (DEAP) as alternative to TPE
```

### 8.3 Time Budget Summary

```
Phase 0: Already done
Phase 1:  14 min  ═══════════════════ 7-param refinement
Phase 2A: 28 min  ════════════════════════════════ FIRST tuning
Phase 2B: 28 min  ════════════════════════════════ SECOND tuning
Phase 3:  28 min  ════════════════════════════════ 14-param joint (optional)
Phase 4:  12 min  ════════════ validation
         ─────
Total:   110 min  (under 2 hours for complete pipeline)
```

### 8.4 Expected Impact

| Phase | FIRST Win Rate | SECOND Win Rate | Total vs Ref |
|:-----:|:-------------:|:---------------:|:------------:|
| Current (post-fix) | 100% | 64% | 82% |
| Phase 1 (7-param @500ms) | 100% | 68% | 84% |
| Phase 2 (separate 14-param) | 100% | **75%** | **88%** |
| Phase 3 (joint 14-param) | 100% | **78%** | **89%** |

**Target**: 85-90% vs reference engines (agent + superchym)
**Stretch goal**: 70%+ SECOND win rate

---

## 9. Implementation Roadmap

### 9.1 C++ Changes Needed

```diff
// board.hpp / board.cpp — Add 14-param support

+ // Thread-local: 7 FIRST + 7 SECOND weights
+ void set_tune_weights_14(int fw[7], int sw[7]) noexcept;
+ // Keep existing 7-param for backward compat

// evaluate() — pick weight set by player
  if (g_tune_active) {
+     const int* w = (player == k_player_us) ? g_tune_fw : g_tune_sw;
+     return score * w[0] + territory_diff * w[1] + ...;
  }
```

```diff
// tuner_cli.cpp — Support 14-param input

+ --weights-first w0 w1 w2 w3 w4 w5 w6
+ --weights-second w0 w1 w2 w3 w4 w5 w6
+ --games N --seed S --time MS

// play_game: use correct weight set per turn
+ if (is_candidate_turn && candidate_is_first)
+     set_tune_weights_14(candidate_fw, baseline_sw);
+ else if (is_candidate_turn && !candidate_is_first)
+     set_tune_weights_14(baseline_fw, candidate_sw);
+ else
+     clear_tune_weights();
```

### 9.2 Python Changes

```diff
# tune_optuna.py — 14-param objective

+ WEIGHT_SPEC_FIRST = [
+     ("f_score", 0, 15, 3), ...
+ ]
+ WEIGHT_SPEC_SECOND = [
+     ("s_score", 0, 15, 3), ...
+ ]

+ def objective(trial, ...):
+     fw = [trial.suggest_int(name, lo, hi) for name, lo, hi, _ in WEIGHT_SPEC_FIRST]
+     sw = [trial.suggest_int(name, lo, hi) for name, lo, hi, _ in WEIGHT_SPEC_SECOND]
+     result = run_tuner_14(fw, sw, ...)
-     # current: single 7-param set
```

### 9.3 Priority Order

```
IMMEDIATE (do first):
  1. Phase 1: Re-run 7-param tuning @500ms (change --time 500)
     → Quick win, minimal code changes
  2. Implement 14-param evaluate() in board.cpp
     → ~2 hours coding + testing

SHORT-TERM (next):
  3. Phase 2A: Tune FIRST weights (7 params)
  4. Phase 2B: Tune SECOND weights (7 params)
  5. Tournament validation

MEDIUM-TERM:
  6. Phase 3: Joint 14-param refinement (optional)
  7. GA alternative implementation for comparison
  8. Adaptive budget strategy (if needed)
```

### 9.4 Risk Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|:----------:|:------:|------------|
| 14-param evaluate() bug | Low | High | Extensive unit tests + baseline validation |
| 500ms too slow for tuning | Low | Medium | Phase 1 verifies before committing to Phase 2 |
| SECOND doesn't improve | Medium | Medium | Try aggressive SECOND-specific tuning |
| TPE doesn't converge in 14D | Medium | High | Phase 2 (separate 7D) is fallback |
| Overfit to baseline | Low | Medium | Always validate vs opponents |

---

## Appendix A: Quick Reference — tuner_cli Commands

```bash
# Phase 1: 7-param refinement
python tools/tune_optuna.py --trials 200 --games 8 --workers 8 --time 500

# Phase 2A: FIRST weights (need 14-param tuner first)
python tools/tune_optuna.py --mode first --trials 200 --games 8 --workers 8 --time 500

# Phase 2B: SECOND weights  
python tools/tune_optuna.py --mode second --trials 200 --games 8 --workers 8 --time 500

# Phase 3: Joint 14-param
python tools/tune_optuna.py --mode joint --trials 200 --games 8 --workers 8 --time 500

# Validation tournament
python scripts/tournament.py
```

## Appendix B: Current best_weights.cfg

```
# Tuned @200ms, 7-param, 20 games/trial
score=15
territory=13
corners=16
edges=8
live_adj=1
recapture=13
vulnerability=-3

margin=+3.60 (baseline=+0.75)

# Key observation: live_adj=1 is very low (baseline=3)
# This suggests the tuner found that live_adj is unreliable at 200ms
# → Confirms that 500ms is needed for reliable adjacency evaluation
```

---

*Last updated: 2026-07-14*  
*Author: Senior Software Architect — Cordyceps Engine Team*
