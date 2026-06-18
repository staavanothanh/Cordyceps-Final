#include "engine/search.hpp"

namespace cordyceps {

SearchResult Search::simple_search(const Board& board, const SideConfig& config) noexcept {
    auto moves = generate_legal_moves_optimized(board, table_);
    int player = board.current_player;

    SearchResult best{k_pass_move, -999999};

    for (const auto& mv : moves) {
        Board tmp = board;
        tmp.current_player = player;
        auto undo = tmp.apply_move(mv);
        int score = evaluate(tmp, player);
        tmp.unmake_move(undo);

        if (score > best.eval) {
            best.eval = score;
            best.move = mv;
        }
    }

    // If no legal moves or losing while opponent passed → pass
    if (moves.empty()) {
        best.move = k_pass_move;
        best.eval = evaluate(board, player);
        return best;
    }

    // If opponent has passed and we're winning → pass to lock win
    if (board.consecutive_passes >= 1) {
        int margin = board.score_from_perspective(player);
        if (margin > 0) {
            best.move = k_pass_move;
            best.eval = evaluate(board, player);
        }
    }

    return best;
}

} // namespace cordyceps
