#ifndef CORDYCEPS_IO_PROTOCOL_HPP
#define CORDYCEPS_IO_PROTOCOL_HPP

#include <string>
#include "common/types.hpp"
#include "engine/board.hpp"

namespace cordyceps {

struct PassTracker {
    bool opp_has_passed{false};
    bool we_have_passed{false};
    int  last_pass_player{0}; // 0=reset, 1=us, -1=opp

    [[nodiscard]] bool is_game_over() const noexcept {
        return opp_has_passed && we_have_passed;
    }

    void reset() noexcept {
        opp_has_passed = false;
        we_have_passed = false;
        last_pass_player = 0;
    }
};

class Protocol {
public:
    Protocol();
    void run();

private:
    Board board_;
    PassTracker pass_tracker_;
    int our_player_{0}; // 1=FIRST, -1=SECOND (set from INIT)
    bool i_am_first_{false};

    void handle_ready();
    void handle_init(const std::string& line);
    void handle_time(const std::string& line);
    void handle_opp(const std::string& line);
    void handle_finish();

    [[nodiscard]] Move pick_random_move() const noexcept;
    [[nodiscard]] bool should_we_pass() const noexcept;

    void write_move(const Move& mv) const;
};

} // namespace cordyceps

#endif // CORDYCEPS_IO_PROTOCOL_HPP
