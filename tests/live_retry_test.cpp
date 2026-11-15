#include "live_retry.hpp"
#include <cassert>

int main() {
    xs::LiveRetryState retry;
    int variant = 0;
    // VOD and normal user exits must never trigger automatic playback.
    assert(!xs::retry_live_after_failure(false, true, 3, variant, retry));
    assert(!xs::retry_live_after_failure(true, false, 3, variant, retry));
    assert(retry.attempts == 0 && variant == 0);

    // Failed sources switch to another server, but cannot loop forever.
    assert(xs::retry_live_after_failure(true, true, 3, variant, retry));
    assert(variant == 1 && retry.attempts == 1);
    assert(xs::retry_live_after_failure(true, true, 3, variant, retry));
    assert(variant == 2 && retry.attempts == 2);
    assert(!xs::retry_live_after_failure(true, true, 3, variant, retry));
    assert(variant == 2);

    // With just one source, a new request can refresh expired stream URLs.
    xs::LiveRetryState single;
    variant = 0;
    assert(xs::retry_live_after_failure(true, true, 1, variant, single));
    assert(xs::retry_live_after_failure(true, true, 1, variant, single));
    assert(variant == 0);
    assert(!xs::retry_live_after_failure(true, true, 1, variant, single));

    // An error response without any sources also gets at most two retries.
    xs::LiveRetryState unavailable;
    assert(xs::retry_live_after_failure(true, true, 0, variant, unavailable));
    assert(xs::retry_live_after_failure(true, true, 0, variant, unavailable));
    assert(!xs::retry_live_after_failure(true, true, 0, variant, unavailable));
    return 0;
}
