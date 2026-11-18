#include "service_request.hpp"
#include "service_diagnostics.hpp"
#include <cassert>
#include <thread>
int main(){
    using namespace std::chrono;
    xs::ServiceRequest queued(milliseconds(0));assert(queued.stopped());
    xs::ServiceRequest live;assert(!live.stopped());assert(live.remaining_ms()<=25000);
    std::thread cancel([&]{live.cancel();});cancel.join();
    assert(live.cancelled()&&live.stopped()&&live.remaining_ms()==0);
    xs::ServiceRequest retry;assert(!retry.cancelled()&&!retry.stopped());
    assert(xs::service_return_code("portal200001")=="portal200001");
    assert(xs::service_return_code("permission-denied")=="permission-denied");
    for(const auto* value:{"token-123456789","portal200001https://secret","portal20000x","private\nvalue"})
        assert(xs::service_return_code(value)=="no reconocido");
}
