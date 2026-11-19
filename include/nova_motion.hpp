#pragma once
#include <chrono>
#include <cstdint>
#include <string>

namespace xs {
enum class MotionMode : int {
    Off = 0,
    SmoothDisplay = 1,
    RifeGpu = 2
};
inline MotionMode nova_motion_mode(int value) {
    return value >= 0 && value <= 2 ? static_cast<MotionMode>(value)
                                   : MotionMode::Off;
}
inline const char* nova_motion_label(MotionMode mode) {
    switch(mode) {
    case MotionMode::SmoothDisplay: return "Suavizado MPV";
    case MotionMode::RifeGpu: return "RIFE GPU Vulkan (2x, experimental)";
    default: return "Desactivado";
    }
}
inline bool nova_motion_needs_gpu(MotionMode mode) {
    return mode==MotionMode::RifeGpu;
}
// A single 450 ms debounce for BOTH quality and motion setting changes.
class NovaPlaybackRestartGate {
public:
    using Clock=std::chrono::steady_clock;
    void observe(std::uint64_t revision,Clock::time_point now) {
        if(revision_!=revision) {
            revision_=revision;
            last_change_=now;
        }
    }
    bool ready(int quality,int motion,int activeQuality,int activeMotion,
               Clock::time_point now)const {
        return ready(quality,motion,0,activeQuality,activeMotion,0,now);
    }
    bool ready(int quality,int motion,int rtxScale,int activeQuality,
               int activeMotion,int activeRtxScale,Clock::time_point now)const {
        return revision_>0 && quality>=0 && quality<=5 &&
               motion>=0 && motion<=2 && rtxScale>=0 && rtxScale<=4 &&
               (quality!=activeQuality || motion!=activeMotion ||
                (quality==5 && rtxScale!=activeRtxScale)) &&
               now-last_change_>=std::chrono::milliseconds(450);
    }
private:
    std::uint64_t revision_=0;
    Clock::time_point last_change_{};
};
}
