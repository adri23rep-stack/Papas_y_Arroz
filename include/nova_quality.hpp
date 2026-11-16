#pragma once
#include "video_enhanced.hpp"
#include <algorithm>
#include <array>
#include <utility>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace xs {
// MPV live-stream policy: 15 seconds of packets before initial playback,
// then retain up to 30 seconds for brief provider/network stalls. Values
// intentionally apply only to live TV, not films or local playback.
inline constexpr std::array<std::pair<const char*,const char*>,7> kNovaLiveCacheOptions{{
    {"cache-pause","yes"},
    {"cache-pause-initial","yes"},
    {"cache-pause-wait","15"},
    {"cache-secs","30"},
    {"demuxer-readahead-secs","30"},
    {"demuxer-max-bytes","128MiB"},
    {"demuxer-max-back-bytes","4MiB"}
}};
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
        return revision_ != 0 && desired >= 0 && desired < 6 &&
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
    return value >= 0 && value <= 5 ? static_cast<VideoPreset>(value)
                                   : VideoPreset::Balanced;
}
inline bool nova_rtx_vsr(VideoPreset preset) {
    return preset == VideoPreset::RtxAiSuperResolution;
}
// Avoid wasting GPU time on a 4K upscaled surface that the monitor cannot show.
// NVIDIA VSR is requested only when the content is smaller than the display.
inline double nova_rtx_vsr_scale(int srcW, int srcH, int screenW, int screenH) {
    if (srcW <= 0 || srcH <= 0 || screenW <= 0 || screenH <= 0) return 1.0;
    const double factor = std::min(double(screenW) / srcW, double(screenH) / srcH);
    return factor >= 1.10 ? std::min(2.0, factor) : 1.0;
}
inline bool nova_needs_shader(VideoPreset preset) {
    return preset == VideoPreset::GpuFsrcnnx;
}
}
