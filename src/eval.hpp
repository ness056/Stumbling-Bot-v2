#pragma once

#include "board.hpp"

namespace StumblingBotV2 {

namespace EvalValues {
    constexpr double pieces[12] = {
        100., -100.,
        300., -300.,
        300., -300.,
        500., -500.,
        900., -900.,
        0.,    0.,
    };

    constexpr double stalemate = 0;
    constexpr double mate = -100'000'000;
    constexpr double mate_depth_factor = -1000;
};

double eval(const Board &);

}