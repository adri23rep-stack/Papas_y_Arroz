#pragma once
#include "video_enhanced.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>

namespace xs {
// Pure policy used by Nova's mpv worker. Rapid requests collapse into a single
// restart, while selecting the already-active preset does not restart anything.
class NovaQualityRestartGate {
public:
    using Clock = std::chrono::steady_clock;
    void observe(std::uint64_t revision, Clock::time_point now) {
        if (revision != revision_) {
            revision_ = revision;
            updated_ = now;
        }
    }
    bool ready(int desired, int active, Clock::time_point now) const {
        return revision_ != 0 && desired >= 0 && desired < 5 &&
               desired != active &&
               now - updated_ >= std::chrono::milliseconds(450);
    }
private:
    std::uint64_t revision_ = 0;
    Clock::time_point updated_{};
};

inline bool nova_can_resume(bool live, double seconds) {
    return !live && std::isfinite(seconds) && seconds > 0.0 && seconds < 86400.0;
}
inline VideoPreset nova_video_preset(int value) {
    return value >= 0 && value <= 4 ? static_cast<VideoPreset>(value)
                                   : VideoPreset::Balanced;
}
inline bool nova_needs_shader(VideoPreset preset) {
    return preset == VideoPreset::GpuFsrcnnx;
}
}
