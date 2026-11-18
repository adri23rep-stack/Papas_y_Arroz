#pragma once
#include <atomic>
#include <chrono>
#include <algorithm>
namespace xs {
// One budget for the complete source chain, including queue time and retries.
// No account data is stored here. Cancellation is safe from the UI thread.
class ServiceRequest {
    using Clock=std::chrono::steady_clock;
    const Clock::time_point deadline_;
    std::atomic<bool> cancelled_{false};
public:
    explicit ServiceRequest(std::chrono::milliseconds timeout=std::chrono::milliseconds(45000))
        :deadline_(Clock::now()+timeout){}
    void cancel(){cancelled_.store(true);}
    bool cancelled() const{return cancelled_.load();}
    long remaining_ms() const{
        if(cancelled())return 0;
        return static_cast<long>(std::clamp<long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline_-Clock::now()).count(),0,25000));
    }
    bool stopped() const{return remaining_ms()==0;}
};
}
