#include "channel_rail.hpp"
#include <cassert>
int main(){
    xs::ChannelRail rail(20,0);
    assert(rail.available()&&rail.hit(30,580)==0);
    rail.press(30,580,100);
    assert(rail.preview(600)==-1);
    assert(rail.preview(660)==0);
    assert(rail.release(30,580,680)==-1); // held: preview only
    rail.press(30,580,800);
    assert(rail.release(30,580,850)==0); // click switches
    rail.press(30,580,900);
    rail.motion(5);
    assert(rail.dragging()&&rail.release(5,580,950)==-1);
    rail.wheel(-4);
    assert(rail.offset()>0);
    rail.center_on(19);
    assert(rail.focus()==19);
    rail.move_focus(-1);
    assert(rail.focus()==18);
    assert(rail.hit(5,580)==-1);
    xs::ChannelRail empty(0);
    assert(!empty.available()&&empty.hit(30,580)==-1);
    xs::ChannelRail single(1);
    assert(!single.available());
    return 0;
}
