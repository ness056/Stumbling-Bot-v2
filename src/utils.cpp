#include "utils.hpp"

namespace StumblingBotV2 {

std::optional<int32_t> str2int32(const std::string &str) {
    try {
        return std::stoi(str);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int64_t> str2int64(const std::string &str) {
    try {
        return std::stoll(str);
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<std::string> split_string(const std::string &str, char sep) {
    std::vector<std::string> parts;
    
    std::string::const_iterator it = str.cbegin(), sep_it;
    while (true) {
        sep_it = std::find(it, str.cend(), sep);
        parts.push_back(std::string(it, sep_it));

        if (sep_it == str.cend()) {
            break;
        }
        it = sep_it + 1;
    };

    return parts;
}

}