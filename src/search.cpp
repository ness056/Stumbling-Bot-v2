#include "search.hpp"

#include <print>
#include <iostream>
#include <optional>
#include <chrono>
#include <algorithm>

using namespace std::chrono_literals;

#include "eval.hpp"
#include "utils.hpp"

namespace StumblingBotV2 {

struct Result {
    double score;
    std::optional<Move> best_move;
};

static bool comp_moves(const Move &move1, const Move &move2) {
    double s1, s2;
    if (move1.captured_piece == Piece::NONE) {
        s1 = 0;
    } else {
        s1 = EvalValues::pieces[(int)colorless_piece(move1.piece)] - EvalValues::pieces[(int)colorless_piece(move1.captured_piece)];
    }

    if (move2.captured_piece == Piece::NONE) {
        s2 = 0;
    } else {
        s2 = EvalValues::pieces[(int)colorless_piece(move2.piece)] - EvalValues::pieces[(int)colorless_piece(move2.captured_piece)];
    }
    
    return s1 < s2;
}

static double quiescence(const SearchArgs &args, const Board &board, double alpha, double beta) {
    if (board.has_flag(BoardFlags::MATE)) {
        return EvalValues::mate;
    }

    MoveContainer moves = board.generate_moves();
    std::sort(moves.begin(), moves.end(), comp_moves);

    if (moves.size() == 0) {
        return EvalValues::stalemate;
    }

    bool any_captures = false;
    double best_score = -std::numeric_limits<double>::infinity();
    for (const Move &move : moves) {
        if (move.captured_piece == Piece::NONE) {
            continue;
        }
        any_captures = true;

        Board copy = board;
        copy.make_move(move);

        double score = -quiescence(args, copy, -beta, -alpha);

        best_score = std::max(best_score, score);
        alpha = std::max(alpha, score);

        if (alpha >= beta) {
            break;
        }
    }

    if (any_captures) {
        return best_score;
    } else {
        return eval(board) * (board.side_to_move() == Side::WHITE ? 1 : -1);
    }
}

static double negamax(const SearchArgs &args, const Board &board, int depth, double alpha, double beta) {
    if (depth == 0) {
        return quiescence(args, board, alpha, beta);
    }

    if (board.has_flag(BoardFlags::MATE)) {
        return EvalValues::mate + depth * EvalValues::mate_depth_factor;
    }

    MoveContainer moves = board.generate_moves();
    std::sort(moves.begin(), moves.end(), comp_moves);

    if (moves.size() == 0) {
        return EvalValues::stalemate;
    }

    double best_score = -std::numeric_limits<double>::infinity();
    for (const Move &move : moves) {
        Board copy = board;
        copy.make_move(move);

        double score = -negamax(args, copy, depth - 1, -beta, -alpha);

        best_score = std::max(best_score, score);
        alpha = std::max(alpha, score);

        if (alpha >= beta) {
            break;
        }
    }

    return best_score;
}

static Result try_depth(const SearchArgs &args, const Board &board, int16_t depth) {
    Result result{ .score = -std::numeric_limits<double>::infinity() };

    double alpha = -std::numeric_limits<double>::infinity();
    double beta = std::numeric_limits<double>::infinity();

    MoveContainer moves = board.generate_moves();
    std::sort(moves.begin(), moves.end(), comp_moves);

    for (const Move &move : moves) {
        Board copy = board;
        copy.make_move(move);

        double score = -negamax(args, copy, depth - 1, -beta, -alpha);

        if (!result.best_move || score > result.score) {
            result.score = score;
            result.best_move = move;
        }

        alpha = std::max(alpha, score);
    }

    return result;
}

static Result best_move(const SearchArgs &args, const Board &board) {
    auto start = std::chrono::high_resolution_clock::now();
    Result result;

    int16_t depth = 4;
    while (depth < 100) {
        auto now = std::chrono::high_resolution_clock::now();
        if (now - start > 200ms) {
            break;
        }

        uci_println("info depth {}", depth);
        result = try_depth(args, board, depth);
        uci_println("info score cp {}", (int)result.score);
        depth++;
    }

    return result;
}

void search(SearchArgs args, Board board) {
    Result result = best_move(args, board);
    Move move;
    if (result.best_move) {
        move = *result.best_move;
    } else {
        uci_println("info string selected fallback move.");
        move = board.generate_moves().at(0);
    }

    uci_println("bestmove {}", move.to_string());
}

}