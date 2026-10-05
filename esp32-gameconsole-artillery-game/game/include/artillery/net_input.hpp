#pragma once

#include <cstdint>

namespace artillery {

/** Rate-limit held aim keys for online play so nudges do not flood the command log. */
struct AimSendClock {
    int hold_ms = 0;
    int since_send_ms = 0;

    static constexpr int kPeriodMs = 48;

    void reset()
    {
        hold_ms = 0;
        since_send_ms = 0;
    }

    /** Returns step (1 or 2) when a nudge should be sent; 0 to skip this frame. */
    int poll(bool holding, bool edge, uint32_t dt_ms)
    {
        if (!holding) {
            reset();
            return 0;
        }
        hold_ms += static_cast<int>(dt_ms);
        since_send_ms += static_cast<int>(dt_ms);
        if (edge || since_send_ms >= kPeriodMs) {
            since_send_ms = 0;
            return hold_ms > 280 ? 2 : 1;
        }
        return 0;
    }
};

}  // namespace artillery
