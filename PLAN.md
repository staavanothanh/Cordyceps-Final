# 🍄 Cordyceps Engine — Kế Hoạch Phát Triển Toàn Diện

> **Mục tiêu**: Xây dựng engine C++20 vô địch NYPC Master Track — Mushroom Game.
> **Ngày lập**: 2026-06-18
> **Trạng thái**: Pre-implementation, hoàn tất nghiên cứu & thiết kế

---

## Mục Lục

1. [Tổng Quan Dự Án](#1-tổng-quan-dự-án)
2. [Luật Chơi & Ràng Buộc BTC](#2-luật-chơi--ràng-buộc-btc)
3. [Phân Tích Đối Thủ](#3-phân-tích-đối-thủ)
4. [Phân Tích Logs BTC](#4-phân-tích-logs-btc)
5. [Chiến Lược FIRST vs SECOND](#5-chiến-lược-first-vs-second)
6. [Kiến Trúc Engine](#6-kiến-trúc-engine)
7. [Thuật Toán Core](#7-thuật-toán-core)
8. [Cấu Trúc Dữ Liệu](#8-cấu-trúc-dữ-liệu)
9. [Kế Hoạch Phát Triển 5 Phase](#9-kế-hoạch-phát-triển-5-phase)
10. [Testing Strategy](#10-testing-strategy)
11. [Rủi Ro & Biện Pháp](#11-rủi-ro--biện-pháp)

---

## 1. Tổng Quan Dự Án

| Thuộc tính | Giá trị |
|-----------|---------|
| **Tên Engine** | Cordyceps Engine |
| **Ngôn ngữ** | C++20 |
| **File nộp** | `main.cpp` (< 1 MiB) + `data.bin` (< 10 MiB) |
| **Dev structure** | Multi-file `src/` (CMake) → `scripts/merge.py` → single-file submission |
| **Compiler (dev)** | MinGW-w64 g++-14 trên **Windows** (code/test/debug/tournament) |
| **Compiler (verify)** | `g++-14 -O3 -std=c++20` trên WSL Ubuntu 24.04 (chỉ verify trước submit) |
| **Thể loại** | 2-player perfect-information territory game |
| **Board** | 10×17 = 170 cells, nấm giá trị 1-9 |

---

## 2. Luật Chơi & Ràng Buộc BTC

### 2.1 Luật chơi

- Chọn hình chữ nhật (r1, c1, r2, c2) có **tổng giá trị nấm = 10**
- **Inscribed Rule**: Cả 4 cạnh của rect phải chạm ít nhất 1 nấm còn sống
- **Pass Move**: `-1 -1 -1 -1`. Game kết thúc khi **cả 2 player pass liên tiếp**
- Tính điểm: tổng giá trị nấm đã thu được

### 2.2 Ràng buộc AWS

| Yếu tố | Giới hạn |
|--------|----------|
| Instance | AWS c7a.2xlarge |
| CPU | **1 Core** |
| RAM | **1,024 MiB (1 GB)** |
| Time/turn | **10,000ms** tổng |
| Network | **Offline** hoàn toàn |

### 2.3 Ràng buộc code

- ❌ Cấm `std::thread`, `std::async`
- ✅ Dùng `std::array`, `.at()` bounds-check
- ✅ `std::unique_ptr` cho tree nodes
- ✅ Timer check thường xuyên trong search loop

### 2.4 BTC Double-Pass Logging Bug

**Quan trọng**: BTC luôn in 2 dòng pass giống hệt nhau cho MỌI lần pass. Đây là bug in log, không phải engine đi 2 nước.

```
OPP -1 -1 -1 -1     ← Pass thật
OPP -1 -1 -1 -1     ← BTC bug: duplicate
```

**Cách xử lý**: Track `last_pass_player` và `opp_has_passed`. Khi thấy dòng thứ 2 từ cùng player → ignore. Chỉ kết thúc game khi **2 player khác nhau** cùng pass.

---

## 3. Phân Tích Đối Thủ

Đã phân tích 4 engine tham khảo qua source code và logs:

### 3.1 old-cdc (C++ Cordyceps)

| Khía cạnh | Chi tiết |
|-----------|----------|
| **Eval chính** | O(legal_moves) dùng geometry tables |
| **Eval nhẹ** | O(1) — popcount bitboard |
| **MoveGen** | Pre-compute 8,415 rects vào `geometry.bin` |
| **rect_id()** | Công thức toán O(1), không hash table |
| **Search** | Alpha-Beta + TT + iterative deepening |
| **ZT** | Zobrist keys pre-computed trong geometry.bin |
| **mquality.bin** | Move quality table |
| **Điểm mạnh** | Kiến trúc geometry table xuất sắc |
| **Bài học** | rect_id_lut bằng toán học, EvalCache approach |

### 3.2 mushroom-bot (Rust)

| Khía cạnh | Chi tiết |
|-----------|----------|
| **Engine** | Negamax α-β (MCTS disabled) |
| **14 modules** | board, bitboard, movegen, eval, search, end_game, opening, timeman, tt, policy, mcts |
| **Bitboard** | 3×u64 ownership |
| **Pruning** | LMR, null-move, razoring, futility, singular extension |
| **Ordering** | TT best → captures → history → killer → PASS |
| **MCTS** | Flat MCTS (không tree), max 256 iter, rollout depth=6 — **TOY** |
| **Opening** | 16 synthesized books, FIRST/SECOND asymmetry |
| **Điểm mạnh** | Hệ thống search đầy đủ, đã tune kỹ |
| **Bài học** | Cached rectangles memoization, cold/warm mode |

### 3.3 superchym (Rust)

| Khía cạnh | Chi tiết |
|-----------|----------|
| **Eval** | **O(1)** EvalCache với 10 counters |
| **EvalCache** | owned, connectivity, corners, edges, live_adjacent × 2 players |
| **Update** | Incremental — chỉ vùng rect + 1 lân cận |
| **Unmake** | Restore toàn bộ cache = O(1) |
| **Fingerprinting** | 8-dim feature, Mahalanobis matching, 16 known styles |
| **SPSA Tuner** | Auto-tune eval weights bằng giải đấu thực |
| **Điểm mạnh** | EvalCache incremental là gold standard |
| **Bài học** | Fingerprinting quá phức tạp cho phase 1, EvalCache là bắt buộc |

### 3.4 agent-i-think-change (C++)

| Khía cạnh | Chi tiết |
|-----------|----------|
| **Eval** | O(170) — quét toàn bộ 170 cells |
| **MoveGen** | 4 nested loops + row-band break |
| **Feature** | territory, connectivity, corners, edges, recapture, vulnerability |
| **Điểm yếu** | Eval quét 170 cells mỗi node → chậm nhất |
| **Bài học** | Cấu trúc module C++ sạch, single-file |

### 3.5 Bảng So Sánh Eval

| Engine | Eval chính | Eval nhẹ | MoveGen |
|--------|-----------|----------|---------|
| **old-cdc** | O(legal_moves) + geometry | **O(1)** popcount | O(R²C²) + rect table |
| **superchym** | **O(1)** EvalCache | — | O(R²C²) + row-band |
| **agent-i-think** | O(170) | — | O(R²C²) + row-band |
| **mushroom-bot** | O(features) | O(bitboard) | O(R²C²) + hash cache |
| **Cordyceps (target)** | **O(1)** EvalCache | — | O(8415) filter + bitmask |

---

## 4. Phân Tích Logs BTC

Đã phân tích 16+ trận đấu mẫu từ `logs/`.

### 4.1 Time Usage Pattern

| Giai đoạn | Engine ổn định | Engine aggressive | Engine greedy |
|-----------|---------------|-------------------|---------------|
| Opening | 150-250ms | 200-700ms | **0-2ms** |
| Midgame | 150-250ms | 400-2400ms | 0-2ms |
| Late game | 0-60ms | 0-60ms | 0ms |
| Critical spike | — | 1000-2400ms | — |
| **Kết quả** | Thắng đều | **Thắng đậm** | Thua |

### 4.2 Phát hiện chính

1. **Engine 0-2ms thua nhiều hơn thắng** — search nông = thua về lâu dài
2. **Engine search sâu (200-500ms) thắng ổn định**
3. **Critical spike (1-2s) quyết định thắng đậm**
4. **Đối thủ pass duplicate do BTC bug** — cần detect và ignore
5. **Pass chiến thuật**: pass khi đang dẫn để khóa win, pass khi hết move
6. **Có engine đốt 2138ms 1 nước** tại critical position → vẫn thắng

### 4.3 Time Budget Khuyến Nghị

```cpp
int time_budget_ms(int remaining_ms, int live_count, int moves_played) {
    if (moves_played < 4)  return 30;             // Early: shallow
    if (live_count <= 12)  return remaining_ms * 0.15; // Endgame
    if (live_count <= 30)  return remaining_ms * 0.12; // Late
    if (is_critical_position()) return remaining_ms * 0.25; // ALL-IN
    return remaining_ms * 0.10;                    // Default mid
}
```

**Critical position detection**:
- Nhiều legal moves (>30) + điểm sát nhau (<8)
- Ít nấm (<20) + điểm sát (<5)
- Ít moves (<5) + đang dẫn (cơ hội lock win)

---

## 5. Chiến Lược FIRST vs SECOND

### 5.1 Bằng chứng từ logs

| Yếu tố | FIRST | SECOND |
|--------|-------|--------|
| **Thắng** | 10/16 trận (62.5%) | 6/16 trận (37.5%) |
| **Time trung bình** | 150-250ms | **300-550ms** |
| **Dạng rect** | Ngang + khối | **Dọc** (vertical strips) |
| **Phong cách** | Balanced / Defensive | **Aggressive / Steal** |

### 5.2 FIRST: Balanced Control

```
FIRST có lợi thế đi trước → snowball tự nhiên
─────────────────────────────────────────────
- Ăn nấm rải rác, tránh tập trung 1 vùng
- Ưu tiên nước "an toàn" — không tạo cơ hội steal
- Search steady 200-400ms
- Mục tiêu: Duy trì lợi thế, ép đối thủ mắc lỗi
- Late game: Chuyển sang search sâu hơn
```

### 5.3 SECOND: Aggressive Comeback

```
SECOND phải lật ngược thế cờ
─────────────────────────────────────────────
- Search SÂU hơn 50-100% (300-550ms từ early)
- Tìm nước ăn NHIỀU nấm nhất
- Ưu tiên DỌC (vertical strips) để steal lãnh thổ
- Chấp nhận rủi ro cao hơn
- Dùng 60-70% time cho 8-10 nước đầu
```

### 5.4 SideConfig

```cpp
struct SideConfig {
    float time_multiplier;
    float aggression;
    float steal_bonus;
    float defense_bonus;
    bool prefer_vertical;
};

// FIRST
constexpr SideConfig FIRST_CONFIG = {
    1.0,  0.4,  0.5,  1.5,  false
};

// SECOND
constexpr SideConfig SECOND_CONFIG = {
    1.5,  0.8,  2.0,  0.6,  true
};
```

---

## 6. Kiến Trúc Engine

### 6.1 Môi Trường Phát Triển

- **Primary**: Windows + MinGW-w64 (g++-14) — code, test, debug, tournament
- **Verify**: WSL Ubuntu 24.04 LTS — chỉ dùng trước khi submit để đảm bảo tương thích BTC
- **Build system**: CMake cho multi-file dev; script merge → single-file submission

### 6.2 Quy Tắc Side-Agnostic

⚠️ **CRITICAL**: Engine sẽ swap side sau mỗi ván. KHÔNG hardcode bất cứ thứ gì cho 1 bên:

- ❌ KHÔNG hardcode `player = 1` (luôn FIRST)
- ❌ KHÔNG hardcode `my_score` chỉ tính từ góc nhìn FIRST
- ❌ KHÔNG hardcode search depth, eval weights, time budget cho 1 bên
- ✅ Dùng `int player` truyền qua mọi hàm (1 = Cordyceps, -1 = opponent)
- ✅ Score luôn từ góc nhìn `player`
- ✅ Eval: `evaluate(board, player)` trả về score từ góc nhìn `player`
- ✅ Search: `negamax(board, depth, alpha, beta, player)` — player là người sắp đi
- ✅ Protocol: `INIT` có trường `FIRST`/`SECOND` để xác định side

### 6.3 File Layout (Multi-file Dev)

```
src/
├── common/
│   ├── types.hpp          Constants, Move, GamePhase, SideConfig
│   └── bitboard.hpp/cpp   3×uint64_t operations
├── engine/
│   ├── board.hpp/cpp      Board + EvalCache + UndoMove + apply/unapply
│   ├── rect_table.hpp/cpp RectTable (8415 rects), rect_id(), load from data.bin
│   ├── movegen.hpp/cpp    Move generation: filter rect table + inscribed check
│   ├── eval.hpp/cpp       Evaluator: O(1) EvalCache read
│   ├── zobrist.hpp/cpp    Zobrist hashing (keys từ data.bin)
│   ├── tt.hpp/cpp         Transposition Table
│   ├── search.hpp/cpp     Negamax α-β + iterative deepening + pruning
│   ├── endgame.hpp/cpp    Endgame exact solver
│   └── timeman.hpp/cpp    TimeManager + critical detection
├── io/
│   ├── protocol.hpp/cpp   READY/INIT/TIME/OPP/FINISH handler
│   └── dataloader.hpp/cpp Load data.bin
├── main.cpp               Entry point
├── gen_geometry.cpp        Offline: generate rect table → data.bin
└── CMakeLists.txt

scripts/
├── merge.py               Merge toàn bộ src/ → single submission.cpp (< 1 MiB)
├── testing_tool.py        Tournament runner (BTC format + FIRST/SECOND tracking)
└── analyze_tournament.py  Phân tích kết quả: win rate theo side, Elo, time usage
```

### 6.2 Data Flow

```
main()
  └─► Protocol.run()
        ├─ INIT → Board::from_input()
        ├─ TIME → search_best_move()
        │         ├─ Phase Detection
        │         ├─ [Early]  Shallow search (depth 2-3)
        │         ├─ [Mid]    Negamax α-β + TT + pruning
        │         ├─ [Late]   Depth-limited
        │         └─ [End]    Exact Negamax solver
        └─ OPP  → apply_opp_move()
```

### 6.3 Module Dependencies

```
Bitboard ─► Board ─► MoveGen ─► Search ─► Protocol ─► main
                │                    │
            EvalCache            EndgameSolver
            Zobrist              TimeManager
                                TranspositionTable
                
data.bin ─► DataLoader ─► RectTable + EvalWeights
```

---

## 7. Thuật Toán Core

### 7.1 Rectangle Table (Pre-computed)

Board 10×17 có chính xác **8,415** hình chữ nhật. Pre-compute offline:

```
gen_geometry (offline) → data.bin:
┌────────────────────────────────────────┐
│ 8415 RectInfo (mỗi rect ~32 bytes)     │
│  ├─ r1, c1, r2, c2: 4 bytes           │
│  ├─ cell_mask: 3×uint64_t (24 bytes)   │
│  └─ area + padding: 4 bytes            │
│  Total: 8415 × 32 ≈ 270 KB            │
├────────────────────────────────────────┤
│ rect_id_lut[r1][c1][r2][c2] → uint16_t │
│ 10×17×10×17 = 28900 × 2 ≈ 58 KB       │
├────────────────────────────────────────┤
│ Eval weights: 7 × int (28 bytes)       │
├────────────────────────────────────────┤
│ Zobrist keys: 1870 × 8 ≈ 15 KB        │
└────────────────────────────────────────┘
Tổng data.bin: ~350 KB (<< 10 MiB)
```

### 7.2 rect_id() — Công Thức Toán O(1)

```cpp
constexpr int COL_SPAN_SUM = COLS * (COLS + 1) / 2;  // 153

int rect_id(int r1, int c1, int r2, int c2) {
    int prior_rows = r1 * ROWS - r1 * (r1 - 1) / 2;
    int id = prior_rows * COL_SPAN_SUM;
    
    int prior_cols = c1 * COLS - c1 * (c1 - 1) / 2;
    id += (ROWS - r1) * prior_cols;
    
    id += (r2 - r1) * (COLS - c1);
    id += c2 - c1;
    return id;  // [0, 8414]
}
```

### 7.3 Move Generation (Dùng Rect Table)

```cpp
std::vector<Move> generate_legal_moves(const Board& board) {
    std::vector<Move> moves;
    for (int id = 0; id < 8415; ++id) {
        const RectInfo& rect = rect_table_[id];
        if (rect_sum(board.prefix, rect) != 10) continue;
        if (!is_inscribed_fast(board, rect)) continue;
        moves.push_back({rect.r1, rect.c1, rect.r2, rect.c2});
    }
    return moves;
}
```

### 7.4 EvalCache — O(1) Evaluation

```cpp
struct EvalCache {
    int16_t my_cells, opp_cells;
    int16_t my_connectivity, opp_connectivity;
    int16_t my_corners, opp_corners;
    int16_t my_edges, opp_edges;
    int16_t my_vulnerable, opp_vulnerable;
    // 10 × 2 bytes = 20 bytes
};

int evaluate(const Board& board) {
    const auto& c = board.cache;
    int territory   = c.my_cells - c.opp_cells;
    int connect     = c.my_connectivity - c.opp_connectivity;
    int corners     = c.my_corners - c.opp_corners;
    int edges       = c.my_edges - c.opp_edges;
    int vuln        = -(c.my_vulnerable - c.opp_vulnerable);
    
    return territory  * W_TERRITORY
         + connect    * W_CONNECTIVITY
         + corners    * W_CORNERS
         + edges      * W_EDGES
         + vuln       * W_VULNERABILITY;
}
```

### 7.5 Apply/Unmake Move

```cpp
struct UndoMove {
    Move mv;
    Bitboard old_my, old_opp, old_live;
    EvalCache old_cache;        // 20 bytes
    uint64_t old_hash;
    int old_live_count;
    int old_my_score, old_opp_score;
    int old_consecutive_passes;
    // ~80 bytes total
};

void apply_move(Board& b, const Move& mv) {
    UndoMove undo;
    undo.mv = mv;
    undo.old_cache = b.cache;
    undo.old_hash = b.hash;
    // ... save other old state
    
    const RectInfo& rect = rect_table_[rect_id(mv)];
    
    // 1. Bitmask XOR
    b.my_mask ^= rect.cell_mask;
    b.live_mask ^= rect.cell_mask;
    
    // 2. Update EvalCache (vùng rect + 1 lân cận)
    update_eval_cache_apply(b, rect);
    
    // 3. Update hash
    b.hash ^= hash_delta(rect, affected_cells);
    
    // 4. Update scores, pass counter, etc.
    b.my_score += rect_sum;
    b.live_count -= rect_area;
    
    return undo;
}

void unmake_move(Board& b, const UndoMove& undo) {
    // Restore = O(1)
    b.my_mask = undo.old_my;
    b.live_mask = undo.old_live;
    b.cache = undo.old_cache;        // O(1)!
    b.hash = undo.old_hash;
    b.live_count = undo.old_live_count;
    b.my_score = undo.old_my_score;
    b.opp_score = undo.old_opp_score;
    b.consecutive_passes = undo.old_consecutive_passes;
}
```

### 7.6 Negamax Alpha-Beta Search

```cpp
int negamax(Board& board, int depth, int alpha, int beta,
            SearchContext& ctx) {
    // 1. Timeout check
    if (ctx.timer.expired()) return evaluate(board);
    
    // 2. Terminal check
    if (board.consecutive_passes >= 2)
        return terminal_score(board);
    
    // 3. TT probe
    if (ctx.use_tt) {
        TTEntry* entry = tt.probe(board.hash);
        if (entry && entry->depth >= depth) {
            if (entry->flag == EXACT) return entry->score;
            if (entry->flag == LOWER)  alpha = max(alpha, entry->score);
            if (entry->flag == UPPER && entry->score <= alpha) 
                return entry->score;
        }
    }
    
    // 4. Leaf node
    if (depth <= 0) return evaluate(board);
    
    // 5. Null-move pruning (depth >= 3)
    if (ctx.use_nmp && depth >= 3 && can_null_move(board)) {
        apply_pass(board);
        int score = -negamax(board, depth - 3, -beta, -beta + 1, ctx);
        unmake_pass(board);
        if (score >= beta) return score;
    }
    
    // 6. Generate & order moves
    auto moves = generate_legal_moves(board);
    moves.push_back(PASS_MOVE);  // PASS luôn là candidate
    order_moves(board, moves, ctx);
    
    // 7. Search loop
    int best_score = -INF;
    Move best_move = PASS_MOVE;
    
    for (int i = 0; i < moves.size(); i++) {
        UndoMove undo = apply_move(board, moves[i]);
        int score = -negamax(board, depth - 1, -beta, -alpha, ctx);
        unmake_move(board, undo);
        
        if (score > best_score) {
            best_score = score;
            best_move = moves[i];
        }
        alpha = max(alpha, score);
        if (alpha >= beta) {
            // Beta cutoff → update killers + history
            update_killer(ctx, depth, moves[i]);
            update_history(ctx, moves[i], depth);
            break;
        }
        
        // Late Move Reduction
        if (i >= 4 && depth >= 3 && !is_tactical(moves[i])) {
            // Reduced search first
        }
    }
    
    // 8. TT store
    tt.store(board.hash, depth, best_score, 
             best_score >= beta ? LOWER : 
             best_score <= original_alpha ? UPPER : EXACT,
             best_move);
    
    return best_score;
}
```

### 7.7 Endgame Exact Solver

```cpp
// Kích hoạt khi live_count <= 12 hoặc legal_moves <= 8
EndgameResult endgame_search(Board& board, int depth, int alpha, int beta) {
    if (board.consecutive_passes >= 2)
        return {terminal_score(board), PASS_MOVE};
    if (depth == 0)
        return {evaluate(board), PASS_MOVE};
    
    auto moves = generate_legal_moves(board);
    moves.push_back(PASS_MOVE);
    
    Move best = PASS_MOVE;
    for (auto& mv : moves) {
        UndoMove undo = apply_move(board, mv);
        auto [score, _] = endgame_search(board, depth - 1, -beta, -alpha);
        score = -score;
        unmake_move(board, undo);
        
        if (score > alpha) { alpha = score; best = mv; }
        if (alpha >= beta) break;
    }
    return {alpha, best};
}
```

### 7.8 Pass Handling (BTC Bug Aware)

```cpp
struct PassTracker {
    int opp_has_passed = false;
    int we_have_passed = false;
    int last_pass_player = 0;  // 0=reset, 1=ta, -1=opp
    
    bool is_game_over() const {
        return opp_has_passed && we_have_passed;
    }
};

void handle_opp_move(int r1, int c1, int r2, int c2) {
    if (r1 == -1) {
        if (pass_tracker.last_pass_player == -1) {
            // BTC BUG: duplicate pass → IGNORE
            return;
        }
        pass_tracker.opp_has_passed = true;
        pass_tracker.last_pass_player = -1;
        board.apply_pass();
    } else {
        pass_tracker.last_pass_player = 0;
        pass_tracker.opp_has_passed = false;
        board.apply_opp_move(r1, c1, r2, c2);
    }
}

bool should_we_pass() {
    if (no_legal_moves()) return true;
    if (pass_tracker.opp_has_passed) {
        if (my_score > opp_score) return true;   // Lock win
        if (my_score == opp_score) return false; // TÌM NƯỚC THẮNG!
        if (my_score < opp_score) return false;  // PHẢI ĐÁNH TIẾP!
    }
    return false;
}
```

---

## 8. Cấu Trúc Dữ Liệu

### 8.1 Bitboard

```cpp
struct Bitboard {
    uint64_t lo;  // cells 0-63
    uint64_t mid; // cells 64-127
    uint64_t hi;  // cells 128-169 (42 bits used)
    
    int popcount() const;
    void set(int idx);
    void clear(int idx);
    bool test(int idx) const;
    Bitboard& operator^=(const Bitboard& other);
    Bitboard& operator&=(const Bitboard& other);
    Bitboard& operator|=(const Bitboard& other);
};
```

### 8.2 Board State

```cpp
struct Board {
    // Grid data
    std::array<int8_t, 170> values;   // 0=empty, 1-9=mushroom
    std::array<int8_t, 170> owners;   // 0=none, 1=us, -1=opp
    
    // Bitboards
    Bitboard my_mask;
    Bitboard opp_mask;
    Bitboard live_mask;
    
    // Scores & state
    int my_score = 0;
    int opp_score = 0;
    int live_count = 0;
    int player = 0;              // 1=us, -1=opp
    int consecutive_passes = 0;
    
    // EvalCache (20 bytes)
    EvalCache cache;
    
    // Hash
    uint64_t hash = 0;
};
```

### 8.3 RectInfo (Pre-computed)

```cpp
struct RectInfo {
    int8_t r1, c1, r2, c2;
    uint8_t area;                // (r2-r1+1)*(c2-c1+1)
    uint8_t padding[3];
    Bitboard cell_mask;          // 3×uint64_t = 24 bytes
    // Tổng: 32 bytes
};
```

### 8.4 UndoMove (Delta)

```cpp
struct UndoMove {
    Move mv;
    Bitboard old_my_mask, old_opp_mask, old_live_mask;
    EvalCache old_cache;
    uint64_t old_hash;
    int16_t old_live_count;
    int16_t old_my_score, old_opp_score;
    int8_t old_consecutive_passes;
    int8_t old_player;
    // ~80 bytes
};
```

### 8.5 Transposition Table Entry

```cpp
struct TTEntry {
    uint64_t hash;      // Full 64-bit Zobrist
    int score;          // Cached evaluation
    int depth;          // Search depth (0-255)
    Move best_move;     // 4 bytes
    uint8_t flag;       // EXACT=0, LOWER=1, UPPER=2
    uint8_t age;        // For replacement
};
```

---

## 9. Kế Hoạch Phát Triển 5 Phase

### Phase 1: Skeleton & Board Foundation (Ngày 1-3)

**Mục tiêu**: Compilable multi-file project, protocol I/O, random valid moves.

| Task | Files | Mô tả |
|------|-------|-------|
| 1.1 | `src/common/types.hpp` | Constants (ROWS=10, COLS=17, CELLS=170), Move struct (r1,c1,r2,c2), PASS_MOVE, `player` convention (1=Cordyceps, -1=opponent) |
| 1.2 | `src/common/bitboard.hpp/cpp` | Bitboard: set/clear/test/popcount/XOR, 3×uint64_t |
| 1.3 | `src/engine/board.hpp/cpp` | Board: values[170], owners[170], bitmasks, apply_move/unmake_move |
| 1.4 | `src/engine/movegen.hpp/cpp` | MoveGen: 4 nested loops + prefix sum + row-band break |
| 1.5 | `src/io/protocol.hpp/cpp` | Protocol: READY/INIT/TIME/OPP/FINISH, side-aware (`player` từ INIT) |
| 1.6 | `src/main.cpp` | Entry point, CMakeLists.txt |
| 1.7 | `scripts/merge.py` | Merge tool: flatten multi-file → single main.cpp |

**Gate kiểm tra**:
- [x] CMake build MinGW zero warnings
- [x] Compile WSL: `g++-14 -O3 -std=c++20` zero warnings
- [x] Pass arena protocol: READY → valid sum=10 inscribed move
- [x] merge.py output compiles cleanly (5 files → 5.0 KiB)
- [x] Không crash, không TLE
- [x] **Side-agnostic**: test với cả FIRST và SECOND role
- [x] 26 unit tests pass (Bitboard: 11, Board: 10, MoveGen: 5)

### ✅ Phase 1 hoàn thành — 2026-06-18

**Deliverables**:
- `src/common/types.hpp` — Constants, Move, SideConfig, GamePhase
- `src/common/bitboard.hpp/cpp` — 3×uint64_t operations
- `src/engine/board.hpp/cpp` — Board + UndoMove + apply/unmake (side-agnostic)
- `src/engine/movegen.hpp/cpp` — 2D prefix sum + inscribed check + row-band break
- `src/io/protocol.hpp/cpp` — PassTracker (BTC bug aware), random move stub
- `src/main.cpp` — Entry point
- `CMakeLists.txt` — CMake/CTest/GoogleTest via FetchContent
- `scripts/merge.py` — Flatten multi-file → single main.cpp (5.0 KiB)
- `tests/unit/test_*.cpp` — 26 tests, 100% pass
- `build/` — cordyceps.exe + cordyceps_tests.exe

---

### Phase 2: MoveGen + Evaluation (Ngày 4-7)

**Mục tiêu**: Fast move generation + static eval. 1-ply greedy bot.

| Task | Files | Mô tả |
|------|-------|-------|
| 2.1 | `src/gen_geometry.cpp` | Offline tool: generate 8415 rects + cell masks → data.bin |
| 2.2 | `src/engine/rect_table.hpp/cpp` | RectTable: load từ data.bin, rect_id() formula, lookup |
| 2.3 | `src/engine/eval.hpp/cpp` | EvalCache struct + incremental update + O(1) evaluate |
| 2.4 | `src/engine/board.hpp/cpp` | UndoMove with EvalCache restore (O(1) unmake) |
| 2.5 | `src/engine/movegen.hpp/cpp` | Optimized: filter rect table + bitmask check |
| 2.6 | `src/engine/search.hpp/cpp` | 1-ply greedy (chọn best eval move theo `player`) |

**Gate kiểm tra**:
- [ ] MoveGen đúng: 100+ board states verified vs brute-force
- [ ] EvalCache matches full eval sau apply/unapply cycle
- [ ] 1-ply bot beats random bot >80% (test cả FIRST và SECOND)
- [ ] Move generation + eval <1ms trên full board
- [ ] Side-agnostic: evaluate trả về score từ góc nhìn `player`

---

### Phase 3: Core Search Engine (Ngày 8-12)

**Mục tiêu**: Negamax α-β + TT + iterative deepening.

| Task | Files | Mô tả |
|------|-------|-------|
| 3.1 | `src/engine/zobrist.hpp/cpp` | Zobrist hashing (keys từ data.bin) |
| 3.2 | `src/engine/tt.hpp/cpp` | Transposition Table (flat open-addressing) |
| 3.3 | `src/engine/search.hpp/cpp` | Negamax α-β cơ bản |
| 3.4 | `src/engine/search.hpp/cpp` | Iterative deepening + aspiration windows |
| 3.5 | `src/engine/search.hpp/cpp` | Null-move pruning (R=2-3, depth ≥ 3) |
| 3.6 | `src/engine/search.hpp/cpp` | Move ordering: TT best → captures → killer → history |
| 3.7 | `src/engine/search.hpp/cpp` | Late Move Reduction (conservative, i ≥ 4) |
| 3.8 | `src/engine/search.hpp/cpp` | Pass handling trong search tree (với `player` parameter) |

**Gate kiểm tra**:
- [ ] Negamax trả về minimax score đúng trên endgame positions
- [ ] TT hit rate >30% at depth 4+
- [ ] Bot depth 4 beats 1-ply >90%
- [ ] Đạt depth 6+ trong 500ms trên full board

---

### Phase 4: Endgame + Time + Side (Ngày 13-16)

**Mục tiêu**: Exact endgame solver, time management, FIRST/SECOND.

| Task | Files | Mô tả |
|------|-------|-------|
| 4.1 | `src/engine/endgame.hpp/cpp` | Endgame exact Negamax solver (live ≤ 12) |
| 4.2 | `src/engine/timeman.hpp/cpp` | TimeManager: adaptive budget + critical detection |
| 4.3 | `src/common/types.hpp` | SideConfig: FIRST vs SECOND asymmetry struct |
| 4.4 | `src/engine/search.hpp/cpp` | Move ordering side-aware (vertical preference cho SECOND) |
| 4.5 | `src/io/protocol.hpp/cpp` | PassTracker: BTC bug handling (detect duplicate) |
| 4.6 | `src/engine/search.hpp/cpp` | Phase detection: Early/Mid/Late/Endgame dispatch |

**Gate kiểm tra**:
- [ ] Endgame solver đúng 100% cho ≤6-cell positions
- [ ] 0 timeout trong 500-game test
- [ ] Pass handling đúng mọi scenario (có BTC bug)
- [ ] SECOND strategy đánh bại FIRST strategy trong self-play

---

### Phase 5: Tuning & Ship (Ngày 17-21)

**Mục tiêu**: Production engine, size compliance, tournament ready.

| Task | Files | Mô tả |
|------|-------|-------|
| 5.1 | `src/engine/eval.hpp/cpp` | Eval weight tuning (grid search / SPSA self-play) |
| 5.2 | `src/engine/search.hpp/cpp` | Performance profiling + hot-path optimization |
| 5.3 | `src/` (all) + `scripts/merge.py` | Size compliance: merge + strip → < 1 MiB |
| 5.4 | `src/` (all) | Defensive: `.at()`, assertions, try-catch |
| 5.5 | `src/gen_geometry.cpp` | Build final data.bin with tuned weights |
| 5.6 | `scripts/testing_tool.py` | Tournament 1000+ games (auto-swap, FIRST/SECOND tracking) |
| 5.7 | `scripts/merge.py` | Final submission package |

**Gate kiểm tra FINAL**:
- [ ] `main.cpp` < 950 KiB, `data.bin` < 9 MiB
- [ ] Compile WSL zero warnings
- [ ] 0 crashes, 0 timeouts in 1000-game stress test
- [ ] >70% win rate vs strongest reference bot
- [ ] Elo target: top-tier Master Track

---

## 10. Testing Strategy

### 10.1 Test Levels

| Level | Công cụ | Mục tiêu |
|-------|---------|----------|
| **Unit** | CTest + assertions | Bitboard, Board, EvalCache, MoveGen, Zobrist |
| **Integration** | testing_tool.py | Protocol I/O, TT, Search correctness |
| **Regression** | Known positions | Endgame solver, pass scenarios |
| **Tournament** | testing_tool.py × 1000 | Win rate, Elo, time compliance |
| **Stress** | Windows/MinGW | Memory, performance |
| **WSL Verify** | Ubuntu 24.04 LTS | Cross-platform (chỉ trước submit) |

### 10.2 Tournament Log Format

Mở rộng format BTC để track FIRST/SECOND riêng:

```
INIT <seed>
FIRST <r1> <c1> <r2> <c2> <ms>
SECOND <r1> <c1> <r2> <c2> <ms>
...
FINISH
SCOREFIRST <score>
SCORESECOND <score>
# Các dòng bổ sung (comment #)
# CORDFIRST <win/loss/draw>  ← Cordyceps là FIRST trong ván này
# SWAP_SIDES                 ← Ván tiếp theo sẽ swap
```

### 10.3 Side Tracking

Sau mỗi tournament, phân tích riêng:

```
                    As FIRST   As SECOND   Overall
─────────────────────────────────────────────────
Win Rate            67%        54%         60.5%
Avg Score           48.2       43.1        45.6
Avg Time/Game       3200ms     4100ms      3650ms
Avg Depth           6.2        5.8         6.0
```

Nếu chênh lệch FIRST/SECOND > 15% → cần tune SideConfig riêng.

### 10.4 Merge Script (merge.py)

```python
# merge.py — Merge all src/ files → single main.cpp for submission
# Usage: pypy3 merge.py src/ submission/main.cpp
#
# Rules:
# - Strip #include "..." và inline nội dung
# - Giữ #include <...> (standard library)
# - Bỏ comments nếu vượt 1 MiB
# - Verify output compiles: g++-14 -O3 -std=c++20 -o main submission/main.cpp
```

### 10.5 Tournament Runner (testing_tool.py)

```python
# Chạy tournament với auto-swap:
# Ván 1: Cordyceps = FIRST,  Opponent = SECOND
# Ván 2: Cordyceps = SECOND, Opponent = FIRST  (cùng seed)
# Ván 3: Cordyceps = FIRST,  Opponent = SECOND (seed mới)
# ...
#
# Output: CSV với các cột:
# seed, cordyceps_side, cordyceps_score, opp_score, 
# cordyceps_time_ms, opp_time_ms, result, moves_count
```

---

## 11. Rủi Ro & Biện Pháp

| Rủi ro | Impact | Mitigation |
|--------|--------|-----------|
| **Inscribed rule sai** | Illegal move → disqualify | Test suite 200+ inscribed cases |
| **TT collision** | Wrong score → bad move | 64-bit hash + full board verify |
| **Pass logic bug** | Thua oan | Dedicated pass test suite |
| **TLE** | Forfeit | Hard stop at 95% budget, emergency break |
| **OOM** | Crash | Dynamic TT sizing, pre-allocate buffers |
| **Size exceeded** | Submission rejected | Monitor continuously, minifier backup |
| **WSL khác BTC** | Compile fail | Exact g++-14, testing_tool.py verify |
| **EvalCache drift** | Incremental != full eval | Assertion checks in debug mode |

---

## Phụ Lục A: Cấu Hình data.bin

```
Offset  Size   Field
─────────────────────────────────────────
0       4      magic: "NYPC"
4       4      version: 1
8       4      rect_count: 8415
12      4      zobrist_count: 1870
─────────────────────────────────────────
16      32*8415  RectInfo array (269,280 bytes)
─────────────────────────────────────────
269,296  2*28900  rect_id_lut (57,800 bytes)
─────────────────────────────────────────
327,096  28      Eval weights (7 × int32)
─────────────────────────────────────────
327,124  8*1870  Zobrist keys (14,960 bytes)
─────────────────────────────────────────
TOTAL:  ~342 KB
```

## Phụ Lục B: Eval Weights Mặc Định

```cpp
// Tham khảo từ agent-i-think-change + old-cdc
constexpr int W_TERRITORY    = 148;  // Mỗi ô sở hữu
constexpr int W_CONNECTIVITY = 19;   // Mỗi cặp kề
constexpr int W_CORNERS      = 18;   // Mỗi góc
constexpr int W_EDGES        = 3;    // Mỗi biên
constexpr int W_VULNERABILITY = 9;   // Mỗi ô có thể bị steal (âm)
constexpr int W_MOBILITY     = 20;   // Số legal moves (dùng 0 hiện tại)
constexpr int W_RECAPTURE    = 39;   // Ô đối thủ có thể steal
```

---

> **Tài liệu này là kế hoạch sống — cập nhật sau mỗi phase khi có dữ liệu thực tế từ testing.**
