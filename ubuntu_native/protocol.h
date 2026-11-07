#pragma once
#include <array>
#include <cstdint>

namespace dt {
constexpr std::uint32_t magic = 0x44545332;
struct Config {
    std::uint32_t version = magic, threshold = 250, count = 3, protect = 1;
    std::array<std::uint32_t, 5> keys{{16, 17, 33, 0, 0}};
};
enum Type : std::uint32_t { Ready = 1, Target = 2, Erased = 3, Error = 4, Paused = 5 };
struct Message { std::uint32_t type; std::int32_t value; };
enum ErrorCode { Stale = 1, Protected = 2, InputUnavailable = 3, SessionDenied = 4 };
inline bool valid(const Config &c) {
    if (c.version != magic || c.threshold < 100 || c.threshold > 1000 ||
        c.count < 1 || c.count > 5 || c.protect > 1) return false;
    constexpr std::array<unsigned, 10> allowed{{16,17,18,19,20,30,31,32,33,34}};
    for (unsigned i = 0; i < c.count; ++i) {
        bool found = false;
        for (auto key : allowed) found |= key == c.keys[i];
        if (!found) return false;
        for (unsigned j = 0; j < i; ++j) if (c.keys[i] == c.keys[j]) return false;
    }
    return true;
}
inline bool modifier(unsigned code) {
    return code == 29 || code == 97 || code == 56 || code == 100 || code == 125 || code == 126;
}
static_assert(sizeof(Config) == 36 && sizeof(Message) == 8);
}
