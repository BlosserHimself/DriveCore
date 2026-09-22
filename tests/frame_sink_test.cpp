#include <cassert>

#include "dc_frame_sink.hpp"

namespace {
    class RecordingSink final : public dc::IFrameSink {
        public:
            void on_frame(const dc::Frame& frame) override {
                received = frame;
            }

            dc::Frame received{};
    };

    void test_frame_contents_cross_interface_unchanged() {
        const dc::Frame sent{
            0x1FFFFFFF,
            {0x01, 0x02, 0xA5, 0xFF, 0x10, 0x20, 0x30, 0x40},
            8,
        };
        RecordingSink sink;

        sink.on_frame(sent);

        assert(sink.received.id == sent.id);
        assert(sink.received.data == sent.data);
        assert(sink.received.length == sent.length);
    }
}

int main() {
    test_frame_contents_cross_interface_unchanged();
}
