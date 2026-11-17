#include "eval.hpp"

#include <bit>

namespace StumblingBotV2 {

double eval(const Board &board) {
    double score = 0.;

    Bitboards bitboards = board.get_bitboards();
    for (Piece piece = Piece::IT_BEGIN; piece != Piece::IT_END; piece++) {
        uint64_t bb = bitboards.get_bitboard(piece);
        score += std::popcount(bb) * EvalValues::pieces[(int)piece];
    }

    return score;
}

}