#pragma once
#include <cstddef>

namespace xs {

// Live channel URLs and their authorization may expire between selection
// and playback. Only failed/ended live streams are eligible for a bounded
// retry; user-initiated exits must never reconnect automatically.
struct LiveRetryState {
    int attempts = 0;
};

inline bool retry_live_after_failure(bool live, bool failed, std::size_t variants,
                                     int& variant, LiveRetryState& state) {
    constexpr int max_retries = 2;
    if (!live || !failed || state.attempts >= max_retries) return false;
    ++state.attempts;
    if (variants > 1) variant = static_cast<int>((static_cast<std::size_t>(variant) + 1) % variants);
    return true;
}
} // namespace xs
