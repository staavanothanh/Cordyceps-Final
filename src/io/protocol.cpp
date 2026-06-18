#include "io/protocol.hpp"
#include "engine/rect_table.hpp"
#include "engine/search.hpp"
#include "engine/zobrist.hpp"
#include <iostream>
#include <sstream>

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
            handle_ready();
        } else if (line.starts_with("INIT")) {
            handle_init(line);
        } else if (line.starts_with("TIME")) {
            handle_time(line);
        } else if (line.starts_with("OPP")) {
            handle_opp(line);
        } else if (line.starts_with("FINISH")) {
            handle_finish();
            break;
        }
    }
}

void Protocol::handle_ready() {
    std::cout << "OK\n" << std::flush;
}

void Protocol::handle_init(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    for (int i = 0; i < 10; ++i) {
        unsigned long long seed_part;
        iss >> seed_part;
    }

    std::string side;
    iss >> side;
    
    i_am_first_ = (side == "FIRST");
    our_player_ = i_am_first_ ? k_player_us : k_player_opp;
    board_.current_player = k_player_us;
    
    pass_tracker_.reset();
    
    std::cout << "OK\n" << std::flush;
}

void Protocol::handle_time(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    int our_time, opp_time;
    iss >> cmd >> our_time >> opp_time;

    SideConfig config{};
    config.time_multiplier = 1.0f;
    config.aggression = i_am_first_ ? 0.3f : 0.7f;
    config.steal_bonus = 1.0f;
    config.defense_bonus = i_am_first_ ? 2.0f : 1.0f;
    config.prefer_vertical = !i_am_first_;

    // Allocate ~80% of remaining time for search
    int search_time_ms = our_time * 8 / 10;
    if (search_time_ms < 50) search_time_ms = 50;
    if (search_time_ms > 9500) search_time_ms = 9500;

    Move best;
    if (search_) {
        auto result = search_->iterative_deepening(board_, search_time_ms, config);
        best = result.move;
    } else {
        best = k_pass_move;
    }

    if (best.is_pass()) {
        pass_tracker_.we_have_passed = true;
        pass_tracker_.last_pass_player = our_player_;
    } else {
        pass_tracker_.reset();
    }

    board_.apply_move(best);
    write_move(best);
}

void Protocol::handle_opp(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd, side;
    int r1, c1, r2, c2, ms;
    iss >> cmd >> side >> r1 >> c1 >> r2 >> c2 >> ms;

    Move opp_move{static_cast<std::int8_t>(r1), static_cast<std::int8_t>(c1),
                  static_cast<std::int8_t>(r2), static_cast<std::int8_t>(c2)};

    if (opp_move.is_pass()) {
        if (pass_tracker_.last_pass_player != k_player_opp) {
            pass_tracker_.opp_has_passed = true;
            pass_tracker_.last_pass_player = k_player_opp;
            board_.consecutive_passes = 1;
        }
    } else {
        pass_tracker_.last_pass_player = 0;
        pass_tracker_.opp_has_passed = false;
        board_.consecutive_passes = 0;
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
