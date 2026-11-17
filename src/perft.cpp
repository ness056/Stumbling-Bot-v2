#include "perft.hpp"

#include <chrono>
#include <print>
#include <algorithm>

namespace StumblingBotV2 {

static const std::vector<std::pair<std::string, std::vector<uint64_t>>> c_tests = {
    std::make_pair<std::string, std::vector<uint64_t>>(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        { 1, 20, 400, 8902, 197'281, 4'865'609, 119'060'324, }
        //  3'195'901'860,
    ),
    std::make_pair<std::string, std::vector<uint64_t>>(
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
        { 1, 48, 2039, 97'862, 4'085'603, 193'690'690, }
        //  8'031'647'685,
    ),
    std::make_pair<std::string, std::vector<uint64_t>>(
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        { 1, 14, 191, 2812, 43'238, 674'624, 11'030'083, 178'633'661, }
        //  3'009'794'393,
    ),
    std::make_pair<std::string, std::vector<uint64_t>>(
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        { 1, 6, 264, 9467, 422'333, 15'833'292, 706'045'033, }
    ),
    std::make_pair<std::string, std::vector<uint64_t>>(
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        { 1, 44, 1486, 62'379, 2'103'487, 89'941'194, }
    ),
    std::make_pair<std::string, std::vector<uint64_t>>(
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        { 1, 46, 2079, 89'890, 3'894'594, 164'075'551, }
        //  6'923'051'137,
    ),
};

static uint64_t perft_helper(const Board &board, int depth) {
    if (depth == 0 || board.has_flag(BoardFlags::ENDED)) {
        return 1;
    }

    MoveContainer moves = board.generate_moves();

    if (depth == 1) {
        return moves.size();
    }

    uint64_t count = 0;
    for (const Move &move : moves) {
        Board copy = board;
        copy.make_move(move);
        count += perft_helper(copy, depth - 1);
    }

    return count;
}

void perft() {
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t total_found = 0;

    for (int i = 0; i < c_tests.size(); i++) {
        const auto &test = c_tests[i];
        Board board;
        if (!board.import_fen(test.first)) {
            throw std::runtime_error(std::format("Failed to import perft position {}", i));
        }
        
        for (int depth = 0; depth < test.second.size(); depth++) {
            auto start_test = std::chrono::high_resolution_clock::now();

            uint64_t expected = test.second[depth];
            uint64_t found = perft_helper(board, depth);
            total_found += found;

            auto end_test = std::chrono::high_resolution_clock::now();
            auto duration = end_test - start_test;
            long long ns = duration.count();
            if (ns == 0) {
                ns = 1;
            }
            std::println(
                "{} Test {} with depth {} done in {}, expected {} found {}. {}M nodes/s",
                expected == found ? "V" : "X",
                i, depth, std::chrono::duration_cast<std::chrono::milliseconds>(duration), expected, found, int((double)found / duration.count() * 1000)
            );
        }

        std::println();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = end - start;
    long long ns = duration.count();
    if (ns == 0) {
        ns = 1;
    }
    std::println("Perft done. Found {} nodes in {}, {}M nodes/s",
        total_found, std::chrono::duration_cast<std::chrono::milliseconds>(end - start), int((double)total_found / duration.count() * 1000));
}

void divide(const Board &board, int depth) {
    MoveContainer moves = board.generate_moves();

    uint64_t total = 0;
    std::vector<std::string> str_list;
    for (const Move &move : moves) {
        Board copy = board;
        copy.make_move(move);

        uint64_t count = perft_helper(copy, depth - 1);
        total += count;
        str_list.push_back(std::format("{}: {}", move.to_string() , count));
    }

    std::sort(str_list.begin(), str_list.end());
    for (const auto &str : str_list) {
        std::println("{}", str);
    }

    std::println("\nTotal nodes: {}", total);
}

}