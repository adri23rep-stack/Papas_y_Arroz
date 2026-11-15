#include "live_tv_recovery.hpp"
#include <cassert>
int main(){
    auto x=xs::retry_live_channel(true,false,true,false,0,2,0);
    assert(x.retry&&x.next_variant==1&&x.next_attempt==1);
    x=xs::retry_live_channel(true,false,true,false,1,2,1);
    assert(x.retry&&x.next_variant==0&&x.delay_ms==800);
    assert(!xs::retry_live_channel(true,false,true,false,0,1,2).retry);
    assert(!xs::retry_live_channel(false,false,true,false,0,1,0).retry);
    assert(!xs::retry_live_channel(true,true,true,false,0,2,0).retry);
    assert(!xs::retry_live_channel(true,false,false,false,0,2,0).retry);
    assert(xs::retry_live_channel(true,false,false,true,0,1,0).retry);
    return 0;
}
