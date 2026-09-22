#pragma once

#include "dc_bus.hpp"
#include "dc_frame_sink.hpp"

namespace dc {
    class BusMonitor {
        public:
            BusMonitor(IBus& bus, IFrameSink& sink)
                : bus_(bus), sink_(sink) {}

            bool poll_once() {
                Frame frame{};
                if (!bus_.poll(frame)) return false;
                sink_.on_frame(frame);
                return true;
            }

        private:
            IBus& bus_;
            IFrameSink& sink_;
    };
}
