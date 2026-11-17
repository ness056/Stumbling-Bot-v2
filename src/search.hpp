#pragma once

#include <atomic>

#include "board.hpp"

namespace StumblingBotV2 {

struct SearchArgs {
    SearchArgs(const std::atomic_bool &stop) : stop(stop) {}

    bool infinite;
    int32_t move_time;

    int32_t white_time;
    int32_t black_time;
    int32_t white_increment;
    int32_t black_increment;
    int16_t moves_to_go;

    std::vector<Move> search_moves;
    int16_t depth;
    int64_t nodes;
    int16_t mate;

    const std::atomic_bool &stop;

    bool ponder;
};

void search(SearchArgs, Board);

}