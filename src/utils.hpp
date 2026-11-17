#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <bit>
#include <print>

namespace StumblingBotV2 {

template <class ...T>
void uci_println(const std::format_string<T...> fmt, T &&...args) {
    std::println(fmt, std::forward<T>(args)...);
    std::fflush(stdout);
}

std::optional<int32_t> str2int32(const std::string &);
std::optional<int64_t> str2int64(const std::string &);

std::vector<std::string> split_string(const std::string &, char sep);

constexpr uint64_t scan_idx_LSB(uint64_t x) {
    if (std::is_constant_evaluated()) {
        return std::countr_zero(x);
    } else {
        return _tzcnt_u64(x);
    }
}

constexpr uint64_t pop_idx_LSB(uint64_t &x) {
    if (std::is_constant_evaluated()) {
        int bit = std::countr_zero(x);
        x &= x - 1;
        return bit;
    } else {
        uint64_t bit = _tzcnt_u64(x);
        x = _blsr_u64(x);
        return bit;
    }
}

constexpr uint64_t scan_bit_LSB(uint64_t x) {
    if (std::is_constant_evaluated()) {
        return x & ((uint64_t)0 - x);
    } else {
        return _blsi_u64(x);
    }
}

constexpr uint64_t pop_bit_LSB(uint64_t &x) {
    if (std::is_constant_evaluated()) {
        uint64_t bit = x & ((uint64_t)0 - x);
        x &= x - 1;
        return bit;
    } else {
        uint64_t bit = _blsi_u64(x);
        x = _blsr_u64(x);
        return bit;
    }
}

}