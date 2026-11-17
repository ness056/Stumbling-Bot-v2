#include "app.hpp"

#include <iostream>
#include <print>
#include <future>

#include "utils.hpp"
#include "perft.hpp"
#include "search.hpp"

namespace StumblingBotV2 {

void App::_command_help(const std::vector<std::string> &) {
    uci_println(
R"(Stumbling Bot v2 is a chess engine made by ness056.
It is written in C++ (https://github.com/ness056/Stumbling-Bot-v2)
and licensed under the MIT License.
It is meant to be used with an external chess GUI.)");
}

void App::_command_uci(const std::vector<std::string> &) {
    uci_println("id name Stumbling Bot v2");
    uci_println("id author ness056");

    uci_println("uciok");
}

void App::_command_isready(const std::vector<std::string> &) {
    uci_println("readyok");
}

void App::_command_setoption(const std::vector<std::string> &) {

}

void App::_command_position(const std::vector<std::string> &tokens) {
    Board new_board;

    if (tokens.size() < 2) {
        uci_println("Command position usage: position [fen <fenstring> | startpos] moves <move1> ... <movei>");
        return;
    }

    std::vector<std::string>::const_iterator moves;
    if (tokens[1] == "startpos") {
        moves = tokens.begin() + 2;
        if (!new_board.import_fen(Board::s_starting_position)) {
            throw std::runtime_error("Failed to import starting position");
        }
    } else if (tokens[1] == "fen") {
        moves = std::find_if(tokens.begin() + 2, tokens.end(), [](const std::string &t) { return t == "moves"; });

        std::vector<std::string> fen_vec(tokens.begin() + 2, moves);
        std::string fen;
        for (const auto &str : fen_vec) {
            fen += str;
            fen += " ";
        }
        if (!new_board.import_fen(fen)) {
            uci_println("Fen string is invalid.");
            return;
        }
    } else {
        uci_println("Command position usage: position [fen <fenstring> | startpos] moves <move1> ... <movei>");
        return;
    }

    if (moves != tokens.end()) {
        for (auto it = moves + 1; it != tokens.end(); it++) {
            if (!new_board.make_move_from_string(*it)) {
                uci_println("Command position usage: position [fen <fenstring> | startpos] moves <move1> ... <movei>");
                return;
            }
        }
    }

    _board = new_board;
}

template<typename T>
static T _command_go_find_arg(
    const std::vector<std::string> &tokens, const std::string &name,
    std::function<std::optional<T>(const std::string &token)> cast_callback, T default_value
) {
    auto it = std::find_if(tokens.begin(), tokens.end(), [&](const std::string &v) { return name == v; });
    if (it == tokens.end()) {
        return default_value;
    }

    if constexpr(std::is_same_v<T, bool>) {
        return true;
    }

    if (++it == tokens.end()) {
        return default_value;
    }

    auto op = cast_callback(*it);
    if (!op) {
        return default_value;
    }
    return *op;
}

void App::_command_go(const std::vector<std::string> &tokens) {
    SearchArgs args(_search_stop);

    args.infinite = _command_go_find_arg<bool>(tokens, "infinite", [](const std::string &) { return false; }, false);
    args.move_time = _command_go_find_arg<int32_t>(tokens, "movetime", [](const std::string &s) { return str2int32(s); }, -1);

    args.white_time = _command_go_find_arg<int32_t>(tokens, "wtime", [](const std::string &s) { return str2int32(s); }, -1);
    args.black_time = _command_go_find_arg<int32_t>(tokens, "btime", [](const std::string &s) { return str2int32(s); }, -1);
    args.white_increment = _command_go_find_arg<int32_t>(tokens, "winc", [](const std::string &s) { return str2int32(s); }, -1);
    args.black_increment = _command_go_find_arg<int32_t>(tokens, "binc", [](const std::string &s) { return str2int32(s); }, -1);
    args.moves_to_go = (int16_t)_command_go_find_arg<int32_t>(tokens, "movestogo", [](const std::string &s) { return str2int32(s); }, -1);

    // args.search_moves = _command_go_find_arg<std::vector<Move>>(tokens, "movestogo", [](const std::string &s) {}, -1);
    args.depth = (int16_t)_command_go_find_arg<int32_t>(tokens, "depth", [](const std::string &s) { return str2int32(s); }, -1);
    args.nodes = _command_go_find_arg<int64_t>(tokens, "nodes", [](const std::string &s) { return str2int64(s); }, -1);
    args.mate = (int16_t)_command_go_find_arg<int32_t>(tokens, "mate", [](const std::string &s) { return str2int32(s); }, -1);

    args.ponder = _command_go_find_arg<bool>(tokens, "ponder", [](const std::string &) { return false; }, false);

    _search_stop = false;
    _search_future = std::async(std::launch::async, search, args, _board);
}

void App::_command_stop(const std::vector<std::string> &) {
    _search_stop = true;
    if (_search_future.valid()) {
        _search_future.wait();
    }
}

void App::_command_draw(const std::vector<std::string> &) {
    _board.draw();
}

void App::_command_perft(const std::vector<std::string> &) {
    perft();
}

void App::_command_divide(const std::vector<std::string> &tokens) {
    if (tokens.size() < 2) {
        uci_println("Command divide usage: divide <depth>");
        return;
    }
    std::optional<int32_t> depth_op = str2int32(tokens[1]);
    if (!depth_op || *depth_op < 0) {
        uci_println("Command divide usage: divide <depth>");
        return;
    }

    int32_t depth = *depth_op;
    divide(_board, depth);
}

int App::run(int argc, char *argv[]) {
    if (argc >= 2 && std::string(argv[1]) == "perft") {
        perft();
        return 0;
    }

    while (true) {
        std::vector<std::string> tokens = _get_tokens();

        if (tokens.size() == 0) {
            continue;
        }

        const std::string &command = tokens[0];
        if (command == "quit") {
            _search_stop = true;
            if (_search_future.valid()) {
                _search_future.wait();
            }
            return 0;
        }
        else if (command == "help") _command_help(tokens);
        else if (command == "uci") _command_uci(tokens);
        else if (command == "isready") _command_isready(tokens);
        else if (command == "setoption") _command_setoption(tokens);
        else if (command == "position") _command_position(tokens);
        else if (command == "go") _command_go(tokens);
        else if (command == "stop") _command_stop(tokens);

        else if (command == "draw") _command_draw(tokens);
        else if (command == "perft") _command_perft(tokens);
        else if (command == "divide") _command_divide(tokens);
        else uci_println("Unknown command {}", command);
    }
}

std::vector<std::string> App::_get_tokens() {    
    std::string line;
    std::getline(std::cin, line);

    return split_string(line, ' ');
}

}