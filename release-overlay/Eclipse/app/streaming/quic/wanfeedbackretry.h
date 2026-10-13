#pragma once
#include <algorithm>

// Called once per telemetry interval. State belongs to one stream session.
class WanFeedbackRetry {
    int remaining = 0;
    int delay = 1;
public:
    bool ready() {
        if (remaining > 0) { --remaining; return false; }
        return true;
    }
    void failed() {
        delay = std::min(30, delay * 2);
        remaining = delay - 1;
    }
    void succeeded() { remaining = 0; delay = 1; }
};
