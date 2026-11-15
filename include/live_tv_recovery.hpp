#pragma once
#include <algorithm>
#include <string>
namespace xs {
struct LiveRetryDecision {
    bool retry=false;
    int next_variant=0;
    int next_attempt=0;
    int delay_ms=0;
};
// Only recover from a failed source or unexpected stream end, never from
// user exit/navigation. Retry is strictly bounded to avoid permanent loops.
inline LiveRetryDecision retry_live_channel(bool live,bool user_navigation,
    bool playback_failed,bool unexpected_eof,int variant,int variants,
    int completed_attempts){
    if(!live||user_navigation||(!playback_failed&&!unexpected_eof)||
       completed_attempts>=2)return {};
    LiveRetryDecision out;out.retry=true;
    out.next_attempt=completed_attempts+1;
    out.next_variant=(variant+1<std::max(1,variants))?variant+1:0;
    out.delay_ms=out.next_attempt==1?300:800;
    return out;
}
}
