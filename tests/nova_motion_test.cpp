#include "nova_motion.hpp"
#include <cassert>
#include <chrono>
#include <string>
int main(){
    using namespace xs;
    assert(nova_motion_mode(-1)==MotionMode::Off);
    assert(nova_motion_mode(3)==MotionMode::Off);
    assert(nova_motion_mode(2)==MotionMode::RifeGpu);
    assert(!nova_motion_needs_gpu(MotionMode::Off));
    assert(!nova_motion_needs_gpu(MotionMode::SmoothDisplay));
    assert(nova_motion_needs_gpu(MotionMode::RifeGpu));
    const auto now=NovaPlaybackRestartGate::Clock::time_point{};
    NovaPlaybackRestartGate gate;
    assert(!gate.ready(1,2,1,0,now+std::chrono::seconds(1)));
    gate.observe(1,now);
    assert(!gate.ready(1,2,1,0,now+std::chrono::milliseconds(449)));
    assert(gate.ready(1,2,1,0,now+std::chrono::milliseconds(450)));
    gate.observe(2,now+std::chrono::milliseconds(300));
    assert(!gate.ready(4,2,1,0,now+std::chrono::milliseconds(700)));
    assert(gate.ready(4,2,1,0,now+std::chrono::milliseconds(750)));
    assert(gate.ready(5,2,1,0,now+std::chrono::milliseconds(750)));
    assert(!gate.ready(6,2,1,0,now+std::chrono::milliseconds(750)));
    assert(!gate.ready(1,0,1,0,now+std::chrono::seconds(2)));
    assert(!gate.ready(1,3,1,0,now+std::chrono::seconds(2)));
    return 0;
}
