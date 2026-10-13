#include "../app/streaming/quic/wanfeedbackretry.h"
#include <cassert>

int main() {
    WanFeedbackRetry retry, other;
    assert(retry.ready());
    for (int delay : {2, 4, 8, 16, 30, 30, 30}) {
        retry.failed();
        assert(other.ready()); // One failed host does not delay another session.
        for (int i = 1; i < delay; ++i) assert(!retry.ready());
        assert(retry.ready()); // Outages never disable feedback permanently.
    }
    retry.succeeded();
    assert(retry.ready());
    retry.failed();
    assert(!retry.ready());
    assert(retry.ready()); // Recovery resets the retry budget.
}
