#pragma once
#include <cstdint>
#include <vector>
#include "dc_topics.hpp"

namespace dc {

    struct Message {
        Category category;
        Topic topic;
        uint8_t source;
        std::vector<uint8_t> payload;
    };
}