#pragma once
#include <chrono>
#include <cstdint>
#include <string>

namespace xs {
enum class MotionMode : int {
    Off = 0,
    SmoothDisplay = 1,
    Compensated60 = 2
};
inline MotionMode nova_motion_mode(int value) {
    return value >= 0 && value <= 2 ? static_cast<MotionMode>(value)
                                   : MotionMode::Off;
}
inline const char* nova_motion_label(MotionMode mode) {
    switch(mode) {
    case MotionMode::SmoothDisplay: return "Suavizado MPV";
    case MotionMode::Compensated60: return "Movimiento 60 FPS (experimental)";
    default: return "Desactivado";
    }
}
inline const char* nova_motion_filter(MotionMode mode) {
    // FFmpeg motion-compensated interpolation. Not RIFE or NVIDIA GPU
    // frame generation: this demanding filter runs mainly on the CPU.
    return mode==MotionMode::Compensated60
        ? "lavfi=[minterpolate=fps=60:mi_mode=mci:mc_mode=obmc:me_mode=bidir]"
        : "";
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
        return revision_>0 && quality>=0 && quality<=4 &&
               motion>=0 && motion<=2 &&
               (quality!=activeQuality || motion!=activeMotion) &&
               now-last_change_>=std::chrono::milliseconds(450);
    }
private:
    std::uint64_t revision_=0;
    Clock::time_point last_change_{};
};
}
