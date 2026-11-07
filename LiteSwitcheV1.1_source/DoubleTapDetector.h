#pragma once
#include <array>
#include <cstdint>

// No WinAPI dependency: timing and key transitions can be tested on any host.
class DoubleTapDetector {
public:
    bool key_down(unsigned key, bool configured, bool modifiers,
                  std::uintptr_t window, std::uint64_t now, unsigned threshold) {
        if (key >= down_.size()) {
            cancel();
            return false;
        }
        if (down_[key]) {
            cancel();
            return false;
        }
        down_[key] = true;
        if (!configured || modifiers || window == 0) {
            cancel();
            return false;
        }
        const bool matched = pending_ && key_ == key && window_ == window &&
            now >= time_ && now - time_ <= threshold;
        cancel();
        if (!matched) {
            pending_ = true;
            key_ = key;
            window_ = window;
            time_ = now;
        }
        return matched;
    }

    void key_up(unsigned key) {
        if (key < down_.size())
            down_[key] = false;
    }

    void cancel() { pending_ = false; }
    void reset() { down_.fill(false); cancel(); }

private:
    std::array<bool, 256> down_{};
    bool pending_ = false;
    unsigned key_ = 0;
    std::uintptr_t window_ = 0;
    std::uint64_t time_ = 0;
};
