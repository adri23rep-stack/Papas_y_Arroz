#include "nova_motion.hpp"
#include <cassert>
#include <chrono>
#include <string>
int main(){
    using namespace xs;
    assert(nova_motion_mode(-1)==MotionMode::Off);
    assert(nova_motion_mode(3)==MotionMode::Off);
    assert(nova_motion_mode(2)==MotionMode::Compensated60);
    assert(std::string(nova_motion_filter(MotionMode::Off)).empty());
    assert(std::string(nova_motion_filter(MotionMode::SmoothDisplay)).empty());
    assert(std::string(nova_motion_filter(MotionMode::Compensated60))
           .find("minterpolate=fps=60")!=std::string::npos);
    const auto now=NovaPlaybackRestartGate::Clock::time_point{};
    NovaPlaybackRestartGate gate;
    assert(!gate.ready(1,2,1,0,now+std::chrono::seconds(1)));
    gate.observe(1,now);
    assert(!gate.ready(1,2,1,0,now+std::chrono::milliseconds(449)));
    assert(gate.ready(1,2,1,0,now+std::chrono::milliseconds(450)));
    gate.observe(2,now+std::chrono::milliseconds(300));
    assert(!gate.ready(4,2,1,0,now+std::chrono::milliseconds(700)));
    assert(gate.ready(4,2,1,0,now+std::chrono::milliseconds(750)));
    assert(!gate.ready(1,0,1,0,now+std::chrono::seconds(2)));
    assert(!gate.ready(1,3,1,0,now+std::chrono::seconds(2)));
    return 0;
}
