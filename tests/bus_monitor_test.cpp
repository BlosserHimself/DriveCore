#include <cassert>
#include <memory>

#include "dc_bus_monitor.hpp"
#include "dc_fake_bus.hpp"

namespace {
    class RecordingSink final : public dc::IFrameSink {
        public:
            void on_frame(const dc::Frame& frame) override {
                ++received_count;
                received = frame;
            }

            uint32_t received_count = 0;
            dc::Frame received{};
    };

    void test_monitor_delivers_one_raw_frame() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender(medium);
        dc::FakeBus receiver(medium);
        assert(sender.start());
        assert(receiver.start());

        RecordingSink sink;
        dc::BusMonitor monitor(receiver, sink);
        const dc::Frame sent{
            0x1FFFFFFF,
            {0x01, 0x02, 0xA5, 0xFF, 0x10, 0x20, 0x30, 0x40},
            8,
        };
        assert(sender.send(sent));

        assert(monitor.poll_once());
        assert(sink.received_count == 1);
        assert(sink.received.id == sent.id);
        assert(sink.received.data == sent.data);
        assert(sink.received.length == sent.length);
        assert(!monitor.poll_once());
        assert(sink.received_count == 1);
    }

    void test_monitor_does_not_manage_bus_lifecycle() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus receiver(medium);
        RecordingSink sink;
        dc::BusMonitor monitor(receiver, sink);

        assert(receiver.state() == dc::BusState::STOPPED);
        assert(!monitor.poll_once());
        assert(receiver.state() == dc::BusState::STOPPED);

        assert(receiver.start());
        assert(receiver.state() == dc::BusState::RUNNING);
        assert(!monitor.poll_once());
        assert(receiver.state() == dc::BusState::RUNNING);

        assert(receiver.stop());
        assert(receiver.state() == dc::BusState::STOPPED);
        assert(!monitor.poll_once());
        assert(receiver.state() == dc::BusState::STOPPED);
        assert(sink.received_count == 0);
    }
}

int main() {
    test_monitor_delivers_one_raw_frame();
    test_monitor_does_not_manage_bus_lifecycle();
}
