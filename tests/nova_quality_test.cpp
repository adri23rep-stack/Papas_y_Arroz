#include "nova_quality.hpp"
#include <cassert>
#include <chrono>
#include <limits>
#include <string>

int main() {
    using namespace xs;
    using Clock = NovaQualityRestartGate::Clock;
    const auto now = Clock::time_point{};
    NovaQualityRestartGate gate;
    assert(!gate.ready(4,1,now+std::chrono::seconds(5)));
    gate.observe(1,now);
    assert(!gate.ready(4,1,now+std::chrono::milliseconds(449)));
    assert(gate.ready(4,1,now+std::chrono::milliseconds(450)));
    gate.observe(2,now+std::chrono::milliseconds(400));
    assert(!gate.ready(3,1,now+std::chrono::milliseconds(700)));
    assert(gate.ready(3,1,now+std::chrono::milliseconds(850)));
    assert(!gate.ready(3,3,now+std::chrono::seconds(3)));
    assert(!gate.ready(-1,1,now+std::chrono::seconds(3)));
    assert(!gate.ready(9,1,now+std::chrono::seconds(3)));
    assert(nova_can_resume(false,5041.25));
    assert(!nova_can_resume(true,5041.25));
    assert(!nova_can_resume(false,-1));
    assert(!nova_can_resume(false,std::numeric_limits<double>::quiet_NaN()));
    assert(!nova_can_resume(false,std::numeric_limits<double>::infinity()));
    assert(nova_video_preset(-5)==VideoPreset::Balanced);
    assert(nova_video_preset(4)==VideoPreset::GpuFsrcnnx);
    assert(nova_needs_shader(VideoPreset::GpuFsrcnnx));
    assert(!nova_needs_shader(VideoPreset::Sharp));
    return 0;
}
