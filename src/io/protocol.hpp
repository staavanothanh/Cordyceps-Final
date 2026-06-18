#ifndef CORDYCEPS_IO_PROTOCOL_HPP
#define CORDYCEPS_IO_PROTOCOL_HPP

#include <string>
#include "common/types.hpp"
#include "engine/board.hpp"

namespace cordyceps {

class RectTable;
class Search;
class Zobrist;

struct PassTracker {
    bool opp_has_passed{false};
    bool we_have_passed{false};
    int  last_pass_player{0};

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
    ~Protocol();
    void run();

private:
    Board board_;
    RectTable* table_{nullptr};
    Zobrist* zobrist_{nullptr};
    Search* search_{nullptr};
    PassTracker pass_tracker_;
    int our_player_{0};
    bool i_am_first_{false};

    void handle_ready();
    void handle_init(const std::string& line);
    void handle_time(const std::string& line);
    void handle_opp(const std::string& line);
    void handle_finish();

    void write_move(const Move& mv) const;
};

} // namespace cordyceps

#endif // CORDYCEPS_IO_PROTOCOL_HPP
