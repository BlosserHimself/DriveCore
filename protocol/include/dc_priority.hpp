#pragma once
#include <cstdint>

namespace dc {
    using Priority = uint8_t;

    // 0 = realtime (typically fastest allowed; carrier may clamp)
    // 1 = 10ms
    // 2 = 100ms
    // 3 = 250ms
    // 4 = 1000ms
    // 5+ = reserved / slower / custom
    namespace priority {
        constexpr Priority REALTIME = 0;
        constexpr Priority MS_10    = 1;
        constexpr Priority MS_100   = 2;
        constexpr Priority MS_250   = 3;
        constexpr Priority MS_1000  = 4;
    }

    constexpr uint32_t priority_to_period_ms(Priority p) {
        switch (p) {
            case priority::REALTIME: return 0;
            case priority::MS_10:    return 10;
            case priority::MS_100:   return 100;
            case priority::MS_250:   return 250;
            case priority::MS_1000:  return 1000;
            default:                 return 1000;
        }
    }
}