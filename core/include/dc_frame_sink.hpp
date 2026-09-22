#pragma once

#include "dc_frame.hpp"

namespace dc {
    class IFrameSink {
        public:
            virtual ~IFrameSink() = default;
            virtual void on_frame(const Frame& frame) = 0;
    };
}
