#pragma once

#include <string>
#include <vector>
#include <future>
#include <atomic>

#include "board.hpp"

namespace StumblingBotV2 {

class App {
public:
    int run(int argc, char *argv[]);

private:
    std::vector<std::string> _get_tokens();

    void _command_help(const std::vector<std::string> &tokens);
    void _command_uci(const std::vector<std::string> &tokens);
    void _command_isready(const std::vector<std::string> &tokens);
    void _command_setoption(const std::vector<std::string> &tokens);
    void _command_ucinewgame(const std::vector<std::string> &tokens);
    void _command_position(const std::vector<std::string> &tokens);
    void _command_go(const std::vector<std::string> &tokens);
    void _command_stop(const std::vector<std::string> &tokens);

    void _command_draw(const std::vector<std::string> &tokens);
    void _command_perft(const std::vector<std::string> &tokens);
    void _command_divide(const std::vector<std::string> &tokens);

    Board _board;
    std::future<void> _search_future;
    std::atomic_bool _search_stop;
};

}