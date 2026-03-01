#pragma once
#include <cstdint>
#include <array>

namespace dc {
    struct Frame {
        uint32_t id;   // 29-bit extended
        std::array<uint8_t, 8> data;
        uint8_t length;
    };
}