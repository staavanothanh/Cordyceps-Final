#include "io/protocol.hpp"
#include "engine/rect_table.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"
#include "engine/timeman.hpp"
#include <iostream>
#include <sstream>
#include <string>

namespace cordyceps {

Protocol::Protocol() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    
    table_ = new RectTable();
    if (table_->load("data.bin")) {
        zobrist_ = new Zobrist();
        search_ = new Search(*table_, *zobrist_);
    }
}

Protocol::~Protocol() {
    delete table_;
    delete search_;
    delete zobrist_;
}

void Protocol::run() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        if (line.starts_with("READY")) {
            handle_ready(line);
        } else if (line.starts_with("INIT")) {
            handle_init(line);
        } else if (line.starts_with("TIME")) {
            handle_time(line);
        } else if (line.starts_with("OPP")) {
            handle_opp(line);
        } else if (line.starts_with("FINISH")) {
            break;
        }
    }
}

void Protocol::handle_ready(const std::string& line) {
    i_am_first_ = (line.find("FIRST") != std::string::npos);
    our_player_ = i_am_first_ ? k_player_us : k_player_opp;
    board_.current_player = k_player_us;
    pass_tracker_.reset();
    std::cout << "OK\n" << std::flush;
}

void Protocol::handle_init(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    board_ = Board{};
    for (int r = 0; r < k_rows; ++r) {
        unsigned long long row_val;
        if (!(iss >> row_val)) break;

        for (int c = k_cols - 1; c >= 0; --c) {
            int digit = static_cast<int>(row_val % 10);
            row_val /= 10;
            int idx = r * k_cols + c;
            board_.values[idx] = static_cast<std::int8_t>(digit);
        }
    }

    board_.recalc_live_mask();
    board_.my_mask = Bitboard::empty();
    board_.opp_mask = Bitboard::empty();
}

void Protocol::handle_time(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    int our_time, opp_time;
    iss >> cmd >> our_time >> opp_time;

    SideConfig config{};
    config.time_multiplier = i_am_first_ ? 1.0f : 1.5f;
    config.aggression = i_am_first_ ? 0.3f : 0.7f;
    config.steal_bonus = 1.0f;
    config.defense_bonus = i_am_first_ ? 2.0f : 1.0f;
    config.prefer_vertical = !i_am_first_;

    TimeManager tm;
    int margin = board_.score_from_perspective(our_player_);
    int live_count = board_.live_count;
    int search_time_ms = tm.get_budget(live_count, config, our_time, margin);

    Move best;
    if (search_) {
        auto result = search_->iterative_deepening(board_, search_time_ms, config);
        best = result.move;
    } else {
        best = k_pass_move;
    }

    if (best.is_pass()) {
        if (pass_tracker_.last_pass_player != our_player_) {
            pass_tracker_.we_have_passed = true;
            pass_tracker_.last_pass_player = our_player_;
        }
    } else {
        pass_tracker_.reset();
    }

    board_.apply_move(best);
    write_move(best);
}

void Protocol::handle_opp(const std::string& line) {
    // OPP format: OPP <r1> <c1> <r2> <c2> <t>
    std::istringstream iss(line);
    std::string cmd;
    int r1, c1, r2, c2, ms;
    iss >> cmd >> r1 >> c1 >> r2 >> c2 >> ms;

    Move opp_move{static_cast<std::int8_t>(r1), static_cast<std::int8_t>(c1),
                  static_cast<std::int8_t>(r2), static_cast<std::int8_t>(c2)};

    if (opp_move.is_pass()) {
        if (pass_tracker_.last_pass_player != k_player_opp) {
            pass_tracker_.opp_has_passed = true;
            pass_tracker_.last_pass_player = k_player_opp;
        }
    } else {
        pass_tracker_.last_pass_player = 0;
        pass_tracker_.opp_has_passed = false;
    }

    board_.apply_move(opp_move);
}

void Protocol::handle_finish() {
}

void Protocol::write_move(const Move& mv) const {
    if (mv.is_pass()) {
        std::cout << "-1 -1 -1 -1\n" << std::flush;
    } else {
        std::cout << static_cast<int>(mv.r1) << ' '
                  << static_cast<int>(mv.c1) << ' '
                  << static_cast<int>(mv.r2) << ' '
                  << static_cast<int>(mv.c2) << '\n' << std::flush;
    }
}

} // namespace cordyceps
