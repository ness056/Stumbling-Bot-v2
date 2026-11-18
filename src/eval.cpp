#include "eval.hpp"

#include <bit>
#include <algorithm>

#include "utils.hpp"

namespace StumblingBotV2 {

static double eval_material(const Board &board) {
    double score = 0.;

    Bitboards bitboards = board.get_bitboards();
    for (Piece piece = Piece::IT_BEGIN; piece != Piece::IT_END; piece++) {
        uint64_t bb = bitboards.get_bitboard(piece);
        score += std::popcount(bb) * EvalValues::pieces[(int)piece];
    }

    return score;
}

static double eval_position(const Board &board) {
    double score = 0.;

    Bitboards bitboards = board.get_bitboards();
    for (Piece piece = Piece::COLORLESS_IT_BEGIN; piece != Piece::COLORLESS_IT_END; next_colorless_piece(piece)) {
        if (piece == Piece::KING) {
            continue;
        }

        uint64_t white_bb = bitboards.get_bitboard(side2piece(Side::WHITE, piece));
        uint64_t black_bb = bitboards.get_bitboard(side2piece(Side::BLACK, piece));

        while (white_bb) {
            Square sq = (Square)pop_idx_LSB(white_bb);
            score += EvalValues::pieces_position[(int)piece / 2][(int)sq];
        }
        while (black_bb) {
            Square sq = (Square)pop_idx_LSB(black_bb);
            score -= EvalValues::pieces_position[(int)piece / 2][(int)sq];
        }
    }

    uint64_t white_king_bb = bitboards.get_bitboard(Piece::WHITE_KING);
    uint64_t black_king_bb = bitboards.get_bitboard(Piece::BLACK_KING);

    float piece_count = (float)std::popcount(bitboards.get_occupancy());
    double early_factor = std::clamp((piece_count - 8.f) / 8.f, 0.f, 1.f);
    double late_factor = 1.f - early_factor;

    score += early_factor * (EvalValues::king_position[0][scan_idx_LSB(white_king_bb)] -
        EvalValues::king_position[0][scan_idx_LSB(black_king_bb)]);
    score += late_factor * (EvalValues::king_position[1][scan_idx_LSB(white_king_bb)] -
        EvalValues::king_position[1][scan_idx_LSB(black_king_bb)]);

    return score;
}

double eval(const Board &board) {
    double score = 0.;

    score += eval_material(board);
    score += eval_position(board);

    return score;
}

}