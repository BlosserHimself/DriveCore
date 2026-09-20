#pragma once
#include <cstdint>

namespace dc {
    using Category = uint8_t;
    using Topic    = uint8_t;

    namespace category {
        constexpr Category SYSTEM = 0x01;
        constexpr Category SENSOR = 0x02;
        constexpr Category UI     = 0x03;
        constexpr Category CLIMATE = 0x04;
    }

    namespace topic {
        namespace system {
            constexpr Topic NODE_ANNOUNCE = 0x01;
            constexpr Topic NODE_ALIVE    = 0x02;
        }

        namespace sensor {
            constexpr Topic RPM     = 0x01;
            constexpr Topic COOLANT = 0x02;
        }

        namespace climate {
            constexpr Topic DRIVER_SEAT_HEAT = 0x01;
        }
    }
}
