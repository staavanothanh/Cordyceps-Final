# Phân Tích & Tính Toán Range Cho Eval Weight Tuning

> **Phân tích bởi**: Senior Software Architect  
> **Ngày**: 2026-07-15  
> **Mục tiêu**: Xác định CHÍNH XÁC range tuning cho 7 eval weights của Cordyceps engine

---

## 1. Cordyceps Evaluate() — Phân Tách Công Thức

### 1.1 Công thức hiện tại

```cpp
eval = score_diff * w_score
    + territory_diff * w_territory
    + corner_diff * w_corners
    + edge_diff * w_edges
    + live_adj_diff * w_live_adj
    + conn_diff * 0          // disabled
    + recapture * 0          // disabled
    + vulnerability * 0;     // disabled
```

### 1.2 Component ranges điển hình trong game

| Component | Công thức | Range lý thuyết | Range thực tế (typical) | Ghi chú |
|-----------|-----------|-----------------|------------------------|---------|
| **score_diff** | Σ(mushroom values ours) − Σ(opp) | 0..765 | 0..50 | Tổng giá trị nấm chênh lệch |
| **territory_diff** | owned_cells_ours − owned_cells_opp | 0..170 | 0..40 | Số ô chiếm chênh lệch |
| **corner_diff** | corners_ours − corners_opp | 0..4 | 0..2 | Tối đa 4 góc |
| **edge_diff** | edges_ours − edges_opp | 0..50 | 0..20 | 50 ô biên (gồm 4 góc) |
| **live_adj_diff** | live_adj_ours − live_adj_opp | 0..680 | 0..40 | Mỗi ô có 4 adj, tối đa 170×4 |
| **connectivity_diff** | conn_ours − conn_opp | 0..340 | 0..10 | Mỗi cạnh kề = 1 connection |

### 1.3 Tầm quan trọng: Component range KHÁC NHAU RẤT NHIỀU!

```
corner_diff range = 4    → weight 1 thay đổi eval tối đa 4
live_adj_diff range ≈ 40 (typical) → weight 1 thay đổi eval tối đa 40
edge_diff range = 50     → weight 1 thay đổi eval tối đa 50
score_diff range ≈ 50    → weight 1 thay đổi eval tối đa 50
territory_diff range ≈ 40 → weight 1 thay đổi eval tối đa 40
```

**Hệ quả**: w_corners PHẢI lớn hơn w_live_adj để có cùng impact.
- w_corners = 8, w_live_adj = 3 → contribution: 8×4=32 vs 3×40=120
- live_adj vẫn có impact gấp ~4× corners dù weight nhỏ hơn!

### 1.4 Baseline contribution breakdown (trong ±200 typical eval)

| Component | Weight | × typical range | Contribution | % |
|-----------|--------|-----------------|-------------|---|
| score | 3 | × 30 | 90 | 35% |
| territory | 3 | × 20 | 60 | 23% |
| corners | 8 | × 1 | 8 | 3% |
| edges | 2 | × 8 | 16 | 6% |
| live_adj | 3 | × 15 | 45 | 17% |
| **Total** | | | **~219** | **~84%** |

---

## 2. Reference Engine Eval Formula Phân Tích

### 2.1 superchym (Rust) — eval range ±5000

**Công thức**:
```
eval = territory * w.territory
    + connectivity * w.connectivity
    + corners * w.corners
    + edges * w.edges
    + recapture_swing * w.recapture      // opp_live_adjacent
    - vulnerability * w.vulnerability     // our_live_adjacent
```

**FIRST weights**: territory=148, mobility=20, connectivity=19, corners=18, edges=3, recapture=39, vulnerability=9

**SECOND weights**: territory=140, mobility=16, connectivity=28, corners=20, edges=6, recapture=28, vulnerability=18

**Contribution breakdown (±5000)**:
| Component | Weight | × typical | Contribution | % of total |
|-----------|--------|-----------|-------------|:----------:|
| territory | 148 | × 15 | 2220 | 53% |
| connectivity | 19 | × 5 | 95 | 2% |
| corners | 18 | × 1 | 18 | 0.4% |
| edges | 3 | × 8 | 24 | 0.6% |
| recapture | 39 | × 5 (opp) | 195 | 5% |
| -vulnerability | -9 | × 5 (our) | -45 | -1% |
| **Total** | | | **~2507** | **~60%** |

> **Note**: superchym có 6 active components. Territory CHIẾM 53% eval → cực kỳ territory-heavy.

### 2.2 mushroom-bot (Rust) — range ±200 (gần nhất với Cordyceps)

**Công thức** (8 components):
```
eval = territory * w.territory
    + safe_territory * (safe_ours - safe_opp)
    + vulnerability * (vuln_ours - vuln_opp)
    + steal_potential * (steal_ours - steal_opp)
    + mobility * (mob_ours - mob_opp)
    + connectivity * (conn_ours - conn_opp)
    + corner_bonus * (corners_ours - corners_opp)
    + edge_bonus * (edges_ours - edges_opp)
```

**Default weights**: territory=154, safe_territory=195, vulnerability=-42, steal_potential=45, mobility=12, connectivity=26, corner_bonus=22, edge_bonus=14

**Overall hybrid** (từ overall.md): territory=154, safe_territory=195, vulnerability=-42, steal_potential=45, mobility=12, connectivity=26, corner_bonus=22, edge_bonus=14

**Balanced**: territory=156, safe_territory=211, vulnerability=-40, steal_potential=40, mobility=8, connectivity=31, corner_bonus=26, edge_bonus=22

**Cold profile**: territory=148, safe_territory=176, vulnerability=-46, steal_potential=39, mobility=18, connectivity=19, corner_bonus=18, edge_bonus=3

### 2.3 agent-i-think-change (C++) — scale gốc ±100

**Weights**: territory=100, connectivity=12, corners=25, edges=5, recapture=50, vulnerability=15

### 2.4 old-cdc (C++) — scale gốc ±5000

**Weights**: territory=170, safe_territory=225, connectivity=55, corners=45, edges=25, mobility=25, steal=160, vulnerability=-95

---

## 3. Scale Factor Calculation

### 3.1 Phương pháp: Eval Range Ratio × Component Count Adjustment

Công thức scale factor chính xác:

```
SF = (R_cordyceps / R_ref) × (N_ref_active / N_cordyceps_active)
```

**Trong đó**:
- R_cordyceps = ±200 (eval range của Cordyceps)
- N_cordyceps_active = 5 (score, territory, corners, edges, live_adj)
- R_ref = eval range của reference engine
- N_ref_active = số active components của reference engine

**Lý do có N_ref/N_cordyceps**: Khi reference engine có NHIỀU components hơn, mỗi component đóng góp ÍT hơn vào tổng eval. Do đó weight cần scale LÊN khi mapping về Cordyceps (có ít components hơn).

### 3.2 Scale Factors

| Reference Engine | Eval Range | Active Comps | Scale Factor (SF) | Công thức |
|:----------------:|:----------:|:------------:|:-----------------:|:---------:|
| **superchym** | ±5000 | 6 | **0.048** | (200/5000)×(6/5) |
| **mushroom-bot** | ±200 | 8 | **1.600** | (200/200)×(8/5) |
| **agent-i-think-change** | ±100 | 5 | **2.000** | (200/100)×(5/5) |
| **old-cdc** | ±5000 | 7 | **0.056** | (200/5000)×(7/5) |

> **Note**: mushroom-bot factor = 1.6 là cao nhất vì nó có 8 components (±200 range) trong khi Cordyceps chỉ có 5. agent-i-think-change factor = 2.0 vì range chỉ ±100.

### 3.3 Component Mapping (Cordyceps ← Reference)

| Cordyceps weight | superchym map | mushroom-bot map | agent-i-t-c map | old-cdc map |
|:----------------:|:-------------:|:----------------:|:---------------:|:-----------:|
| **score** | — (no equivalent) | — | — | — |
| **territory** | w.territory | w.territory | territory | territory |
| **corners** | w.corners | w.corner_bonus | corners | corners |
| **edges** | w.edges | w.edge_bonus | edges | edges |
| **live_adj** | (recapture+vulnerability)* | (steal+vulnerability)* | (recapture+vulnerability)* | (steal+vulnerability)* |
| **recapture** | w.recapture | w.steal_potential | recapture | steal |
| **vulnerability** | w.vulnerability | w.vulnerability | vulnerability | vulnerability |

*\*live_adj = tổ hợp của recapture và vulnerability vì cả 2 đều liên quan đến live adjacency.*

### 3.4 Scaled Weights — Từng Reference Engine

#### superchym (SF = 0.048)

| Weight | FIRST raw | × SF | ≈ Cordyceps | SECOND raw | × SF | ≈ Cordyceps |
|--------|:---------:|:----:|:-----------:|:----------:|:----:|:-----------:|
| territory | 148 | ×0.048 | **7.1** | 140 | ×0.048 | **6.7** |
| corners | 18 | ×0.048 | **0.86** | 20 | ×0.048 | **0.96** |
| edges | 3 | ×0.048 | **0.14** | 6 | ×0.048 | **0.29** |
| live_adj* | (39+9)=48 | ×0.048 | **2.3** | (28+18)=46 | ×0.048 | **2.2** |
| recapture | 39 | ×0.048 | **1.87** | 28 | ×0.048 | **1.34** |
| vulnerability | 9 | ×0.048 | **0.43** | 18 | ×0.048 | **0.86** |

*\*live_adj = (recapture_raw + vulnerability_raw) / 2, rồi × SF*

#### mushroom-bot (SF = 1.6)

| Weight | Default raw | × SF | ≈ Cordyceps | Balanced raw | × SF | ≈ Cordyceps | Cold raw | × SF | ≈ Cordyceps |
|--------|:-----------:|:----:|:-----------:|:-----------:|:----:|:-----------:|:-------:|:----:|:-----------:|
| territory | 154 | ×1.6 | **246** | 156 | ×1.6 | **250** | 148 | ×1.6 | **237** |
| corners | 22 | ×1.6 | **35** | 26 | ×1.6 | **42** | 18 | ×1.6 | **29** |
| edges | 14 | ×1.6 | **22** | 22 | ×1.6 | **35** | 3 | ×1.6 | **5** |
| live_adj* | — | — | — | — | — | — | — | — | — |
| recapture(steal) | 45 | ×1.6 | **72** | 40 | ×1.6 | **64** | 39 | ×1.6 | **62** |
| vulnerability | -42 | ×1.6 | **-67** | -40 | ×1.6 | **-64** | -46 | ×1.6 | **-74** |

*\*mushroom-bot không có live_adj trực tiếp — dùng steal_potential (recapture) và vulnerability*

#### agent-i-think-change (SF = 2.0)

| Weight | Raw | × SF | ≈ Cordyceps |
|--------|:---:|:----:|:-----------:|
| territory | 100 | ×2.0 | **200** |
| corners | 25 | ×2.0 | **50** |
| edges | 5 | ×2.0 | **10** |
| live_adj* | (50+15)=32.5 | ×2.0 | **65** |
| recapture | 50 | ×2.0 | **100** |
| vulnerability | 15 | ×2.0 | **30** |

#### old-cdc (SF = 0.056)

| Weight | Raw | × SF | ≈ Cordyceps |
|--------|:---:|:----:|:-----------:|
| territory | 170 | ×0.056 | **9.5** |
| corners | 45 | ×0.056 | **2.5** |
| edges | 25 | ×0.056 | **1.4** |
| live_adj* | (160+95)=127.5 | ×0.056 | **7.1** |
| recapture(steal) | 160 | ×0.056 | **8.96** |
| vulnerability | -95 | ×0.056 | **-5.3** |

---

## 4. Tính Range Cho Từng Weight

### 4.1 Tổng hợp tất cả scaled values (làm tròn đến integer)

| Cordyceps Weight | superchym F | superchym S | mushroom default | mushroom balanced | mushroom cold | agent-i-t-c | old-cdc | **MIN** | **MAX** |
|:----------------:|:-----------:|:-----------:|:----------------:|:-----------------:|:-------------:|:-----------:|:-------:|:-------:|:-------:|
| **territory** | 7 | 7 | 246 | 250 | 237 | 200 | 10 | **7** | **250** |
| **corners** | 1 | 1 | 35 | 42 | 29 | 50 | 3 | **1** | **50** |
| **edges** | 0 | 0 | 22 | 35 | 5 | 10 | 1 | **0** | **35** |
| **live_adj** | 2 | 2 | — | — | — | 65 | 7 | **2** | **65** |
| **recapture** | 2 | 1 | 72 | 64 | 62 | 100 | 9 | **1** | **100** |
| **vulnerability** | 0 | 1 | -67 | -64 | -74 | 30 | -5 | **-74** | **30** |
| **score** | — | — | — | — | — | — | — | **—** | **—** |

### 4.2 Nhận xét quan trọng

**a) territory range rất lớn (7..250)**: 
- mushroom-bot scaled weights rất cao (237-250) vì territory là component DOMINANT (53-62% eval)
- superchym scaled thấp (7) vì territory được chia sẻ tầm quan trọng với score trong Cordyceps
- **Kết luận**: territory cần range rộng. Current 0-15 KHÔNG đủ!

**b) corner range (1..50)**:
- mushroom-bot scaled: 29-42
- agent-i-t-c: 50
- Current range 0-25 gần đủ, cần mở rộng lên 0-55

**c) edge range (0..35)**:
- mushroom-bot balanced scaled: 35
- Current range 0-10 KHÔNG đủ! Cần mở rộng

**d) live_adj range (2..65)**:
- Không có direct mapping từ mushroom-bot
- agent-i-t-c scaled: 65
- Current range -5..10: KHÔNG đủ upper bound

**e) recapture range (1..100)**:
- agent-i-t-c: 100
- mushroom-bot scaled: 62-72
- Current range 0-15: KHÔNG ĐỦ!

**f) vulnerability range (-74..30)**:
- mushroom-bot: -64 đến -74 (âm!)
- agent-i-t-c: 30 (dương!)
- old-cdc: -5 (âm nhẹ)
- Current range -10..5: KHÔNG đủ cả 2 phía!

**g) score**: Không có reference equivalent. Giữ nguyên dựa trên component range analysis.

### 4.3 Component Range Analysis cho Score

Score_diff range typical = 0-50. Baseline weight=3 → contribution 150/219 = 68%.

Để score có thể đóng góp từ 20% đến 80% eval:
- 20%: score_contrib = 0.2×200=40, w=40/30=1.3 → w min ≈ 0
- 80%: score_contrib = 0.8×200=160, w=160/30=5.3 → w max ≈ 8

Vậy range score: **0-8** (thu hẹp từ 0-15 hiện tại)

---

## 5. Range Mới Đề Xuất

### 5.1 So sánh Range Hiện Tại vs Range Mới

| Weight | Range Hiện Tại | Range Mới (Từ Reference) | Range Mới (Đề Xuất) | Ghi Chú |
|--------|:--------------:|:------------------------:|:------------------:|:-------:|
| **score** | 0..15 | 0..8 (component analysis) | **0..10** | Thu hẹp, score đã chiếm 35% eval |
| **territory** | 0..15 | 7..250 | **0..30** | Mở rộng! Territory quan trọng |
| **corners** | 0..25 | 1..50 | **0..55** | Mở rộng! Corners cần weight lớn |
| **edges** | 0..10 | 0..35 | **0..40** | Mở rộng! |
| **live_adj** | -5..10 | 2..65 | **-10..70** | Mở rộng! Range rất rộng |
| **recapture** | 0..15 | 1..100 | **0..100** | MỞ RỘNG NHIỀU! |
| **vulnerability** | -10..5 | -74..30 | **-80..35** | MỞ RỘNG CẢ 2 PHÍA! |

### 5.2 Range Mới: Detailed Justification

| Weight | Min | Max | Baseline | Lý do Min | Lý do Max |
|--------|:---:|:---:|:--------:|:---------:|:---------:|
| **score** | 0 | 10 | 3 | Có thể tắt score hoàn toàn | Component analysis: 80% eval với score_diff=30 → w=5.3. Tăng lên 10 cho safety. |
| **territory** | 0 | 30 | 3 | Có thể bỏ qua territory | mushroom-bot scaled: 250, nhưng Cordyceps có score term nên territory cần ít hơn. 30 = 10× baseline |
| **corners** | 0 | 55 | 8 | Có thể bỏ qua corners | agent-i-t-c scaled: 50. Thêm safety: 55. |
| **edges** | 0 | 40 | 2 | Có thể bỏ qua edges | mushroom-bot balanced scaled: 35. Thêm safety: 40. |
| **live_adj** | -10 | 70 | 3 | Có thể penalty live_adj (âm) | agent-i-t-c scaled: 65. Thêm safety: 70. |
| **recapture** | 0 | 100 | 0 | Có thể bỏ qua | agent-i-t-c scaled: 100. Giữ nguyên. |
| **vulnerability** | -80 | 35 | 0 | mushroom-bot scaled: -74. Thêm safety: -80 | agent-i-t-c: +30. Thêm safety: 35. |

### 5.3 Revised Search Space

**Range cũ**: 16×16×26×11×16×16×16 = **~3.66B combinations/side**

**Range mới**: 11×31×56×41×81×101×116 = **~7.5×10¹² combinations/side**

> **Tăng ~2000× so với range cũ!**

**14D total (FIRST+SECOND)**: (7.5×10¹²)² ≈ **5.6×10²⁵**

### 5.4 Kết luận về Range

**Range mới RỘNG HƠN NHIỀU so với range cũ.** Điều này có 2 mặt:
1. **Tốt**: Bao phủ được tất cả reference engine strategies
2. **Xấu**: Search space lớn hơn, cần nhiều trials hơn để hội tụ

Tuy nhiên, Optuna TPE được thiết kế cho high-dimensional sparse search. Với multivariate TPE, số trials cần chỉ tăng logarithmic với kích thước search space (O(log N) thay vì O(N)).

---

## 6. Statistical Power Analysis (Tính Lại)

### 6.1 Minimum Trials (Brownlee's Rule)

Brownlee's Rule: **10-50× dimensionality** cho minimum trials.

| Dimensionality | Range Cũ (7D) | Range Mới (7D) | 
|:--------------:|:-------------:|:--------------:|
| 10× | 70 trials | **70 trials** |
| 15× (recommended) | 105 trials | **105 trials** |
| 50× (high confidence) | 350 trials | **350 trials** |

> **Dimensionality KHÔNG thay đổi (vẫn 7D)** nên minimum trials giữ nguyên!
> Chỉ có số lượng giá trị mỗi chiều tăng, không phải số chiều.

### 6.2 Games per Trial (Power Analysis)

**Effect size = +5 eval points** (mức cải thiện tối thiểu有意义)

**Parameters**:
- α = 0.05 (Type I error)
- Power = 0.80 (1-β, Type II error)
- Effect size = 5 (eval points)
- σ = 7.5 (std dev từ baseline self-play data)

**Minimum detectable effect** = 2.77 × σ / √n
- n=4: 2.77 × 7.5/2 = **10.4 points** (quá lớn! Chỉ phát hiện effect >10)
- n=8: 2.77 × 7.5/2.83 = **7.3 points**
- n=16: 2.77 × 7.5/4 = **5.2 points** ✓ (đủ phát hiện effect 5 points)

**Với range mới rộng hơn**: Signal-to-noise ratio có thể giảm vì weight combination quality variance tăng.

| Games/Trial | MDE (points) | Với Range Cũ | Với Range Mới |
|:-----------:|:------------:|:------------:|:-------------:|
| 4 | 10.4 | OK (range hẹp) | CÓ THỂ THIẾU (range rộng, signal yếu hơn) |
| 8 | 7.3 | OK | OK |
| **12** | **6.0** | Dư | **ĐỦ** |
| 16 | 5.2 | Dư | Tốt |

> **Khuyến nghị**: Tăng từ 4 games/trial lên **8-12 games/trial** cho range mới.

### 6.3 Thời Gian Ước Tính

**Giả định**: 8 workers, 500ms/move, ~40 moves/game

| Games/Trial | Time/Trial | 100 Trials | 200 Trials | 350 Trials |
|:-----------:|:----------:|:----------:|:----------:|:----------:|
| 4 | ~80s | ~17 min | ~33 min | ~58 min |
| 8 | ~160s | ~33 min | ~67 min | ~117 min |
| **12 (recommended)** | **~240s** | **~50 min** | **~100 min** | **~175 min** |
| 16 | ~320s | ~67 min | ~133 min | ~233 min |

### 6.4 Khuyến nghị cuối

| Parameter | Range Cũ | Range Mới (Đề Xuất) |
|:---------:|:--------:|:------------------:|
| Range | score(0-15), territory(0-15), corners(0-25), edges(0-10), live_adj(-5-10), recapture(0-15), vulnerability(-10-5) | score(0-10), territory(0-30), corners(0-55), edges(0-40), live_adj(-10-70), recapture(0-100), vulnerability(-80-35) |
| Games/trial | 4 | **12** |
| Trials | 200 | **200** (không đổi) |
| Workers | 8 | 8 |
| Thời gian | ~25 min | **~100 min** |
| Brownlee Rule | Đạt (200 > 70) | Đạt (200 > 70) |
| Power (effect 5) | Gần đủ (4 games: MDE=10.4) | **Đủ (12 games: MDE=6.0)** |

---

## 7. Chiến Lược Tuning Với Range Mới

### 7.1 Tuning 2-Phase

Vì range mới rộng hơn nhiều, đề xuất tuning 2-phase:

**Phase 1: Wide exploration** (50 trials)
- Range đầy đủ như đề xuất
- 8 games/trial, 500ms/move
- Mục tiêu: Xác định vùng optimal gần đúng (khoanh vùng)

**Phase 2: Focused refinement** (150 trials)
- Thu hẹp range dựa trên Phase 1 results
- 12 games/trial, 500ms/move
- Mục tiêu: Tìm optimal weights chính xác

### 7.2 Step Size

Với range rộng:
- **step=2** cho Phase 1 (giảm search space 4×)
- **step=1** cho Phase 2 (độ chính xác cao)

### 7.3 Monitoring

Key metrics cần theo dõi:
- **Margin improvement**: > +5 so với baseline là significant
- **Validation margin**: > +3 trên holdout boards
- **Weight variance**: Nếu weight optimal luôn ở biên → cần mở rộng range thêm

---

## 8. Tổng Kết

### Range Hiện Tại (cần UPDATE)

```
score:         0..15   →    0..10   (thu hẹp)
territory:     0..15   →    0..30   (MỞ RỘNG)
corners:       0..25   →    0..55   (MỞ RỘNG)
edges:         0..10   →    0..40   (MỞ RỘNG)
live_adj:     -5..10   →  -10..70   (MỞ RỘNG)
recapture:     0..15   →    0..100  (MỞ RỘNG NHIỀU)
vulnerability: -10..5  →  -80..35   (MỞ RỘNG CẢ 2 PHÍA)
```

### Thay đổi chính

1. **Range territory mở rộng 2×** (0-15 → 0-30) vì reference engines đặt territory rất cao
2. **Range corners mở rộng 2.2×** (0-25 → 0-55) vì agent-i-t-c và mushroom-bot dùng corners lớn
3. **Range edges mở rộng 4×** (0-10 → 0-40) vì edge_bonus trong mushroom-bot rất cao
4. **Range live_adj mở rộng** cả 2 phía (-5..10 → -10..70) vì reference có live_adj đến 65
5. **Range recapture mở rộng 6.7×** (0-15 → 0-100) — đây là thay đổi lớn nhất
6. **Range vulnerability mở rộng** (-10..5 → -80..35) vì mushroom-bot dùng vulnerability âm rất mạnh
7. **Range score thu hẹp** (0-15 → 0-10) vì score đã chiếm 35% eval ở baseline, không cần range quá rộng

### Games & Time

| Metric | Giá Trị |
|--------|:-------:|
| Games/trial | **12** (từ 4) |
| Trials | 200 |
| Workers | 8 |
| Time ước tính | **~100 phút** (từ ~25 phút) |
| MDE (effect 5) | **6.0 points** ✓ |

---

*Analysis by Senior Software Architect — Dựa trên dữ liệu từ 4 reference engines + Cordyceps codebase*
