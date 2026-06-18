#include "io/protocol.hpp"
#include "engine/movegen.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>

namespace cordyceps {

Protocol::Protocol() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
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
    // INIT <seed10digits>... <side>
    // Format: "INIT <10 seeds> FIRST|SECOND"
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd; // "INIT"

    // Read 10 seed values
    for (int i = 0; i < 10; ++i) {
        unsigned long long seed_part;
        iss >> seed_part;
    }

    // Read side
    std::string side;
    iss >> side;
    
    i_am_first_ = (side == "FIRST");
    our_player_ = i_am_first_ ? k_player_us : k_player_opp;
    board_.current_player = k_player_us; // FIRST always starts
    
    pass_tracker_.reset();
    
    std::cout << "OK\n" << std::flush;
}

void Protocol::handle_time(const std::string& line) {
    // TIME <our_ms> <opp_ms>
    std::istringstream iss(line);
    std::string cmd;
    int our_time, opp_time;
    iss >> cmd >> our_time >> opp_time;

    // Check if opponent has passed and we should pass too
    if (should_we_pass()) {
        write_move(k_pass_move);
        board_.apply_move(k_pass_move);
        pass_tracker_.we_have_passed = true;
        return;
    }

    // Phase 1: random move
    Move best = pick_random_move();
    
    if (best.is_pass()) {
        pass_tracker_.we_have_passed = true;
    } else {
        pass_tracker_.reset();
    }
    
    board_.apply_move(best);
    write_move(best);
}

void Protocol::handle_opp(const std::string& line) {
    // OPP <side> <r1> <c1> <r2> <c2> <ms>
    std::istringstream iss(line);
    std::string cmd, side;
    int r1, c1, r2, c2, ms;
    iss >> cmd >> side >> r1 >> c1 >> r2 >> c2 >> ms;

    Move opp_move{static_cast<std::int8_t>(r1), static_cast<std::int8_t>(c1),
                  static_cast<std::int8_t>(r2), static_cast<std::int8_t>(c2)};

    if (opp_move.is_pass()) {
        // BTC BUG: detect duplicate pass from same player
        if (pass_tracker_.last_pass_player == -1) {
            return; // Ignore duplicate
        }
        pass_tracker_.opp_has_passed = true;
        pass_tracker_.last_pass_player = -1;
    } else {
        pass_tracker_.last_pass_player = 0;
        pass_tracker_.opp_has_passed = false;
    }
    
    board_.apply_move(opp_move);
}

void Protocol::handle_finish() {
    // Clean exit
}

Move Protocol::pick_random_move() const noexcept {
    auto moves = generate_legal_moves(board_);
    
    if (moves.empty()) {
        return k_pass_move;
    }
    
    // Thread-local RNG for deterministic-ish behavior
    static thread_local std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
    return moves[dist(rng)];
}

bool Protocol::should_we_pass() const noexcept {
    if (pass_tracker_.opp_has_passed) {
        int margin = board_.score_from_perspective(our_player_);
        if (margin > 0) return true;      // Lock win
        if (margin <= 0) return false;    // Tied/losing → keep fighting
    }
    return false;
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
