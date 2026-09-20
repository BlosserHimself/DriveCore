#include <cassert>
#include <memory>

#include "dc_fake_bus.hpp"

namespace {
    dc::Frame make_frame(uint32_t id, uint8_t value) {
        dc::Frame frame{};
        frame.id = id;
        frame.data[0] = value;
        frame.length = 1;
        return frame;
    }

    void test_broadcast_and_sender_exclusion() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus first(medium);
        dc::FakeBus second(medium);
        dc::FakeBus third(medium);
        assert(first.start());
        assert(second.start());
        assert(third.start());

        const dc::Frame sent = make_frame(0x123, 0x5A);
        assert(first.send(sent));

        dc::Frame received{};
        assert(!first.poll(received));
        assert(second.poll(received));
        assert(received.id == sent.id);
        assert(received.data == sent.data);
        assert(received.length == sent.length);
        assert(third.poll(received));
        assert(received.id == sent.id);
        assert(received.data == sent.data);
        assert(received.length == sent.length);
    }

    void test_fifo_delivery_and_stats() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender(medium);
        dc::FakeBus receiver(medium);
        assert(sender.start());
        assert(receiver.start());

        assert(sender.send(make_frame(1, 0x10)));
        assert(sender.send(make_frame(2, 0x20)));

        dc::Frame received{};
        assert(receiver.poll(received));
        assert(received.id == 1);
        assert(received.data[0] == 0x10);
        assert(receiver.poll(received));
        assert(received.id == 2);
        assert(received.data[0] == 0x20);
        assert(!receiver.poll(received));
        assert(sender.stats().tx_frames == 2);
        assert(receiver.stats().rx_frames == 2);
    }

    void test_stopped_endpoints_are_offline() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender(medium);
        dc::FakeBus receiver(medium);
        assert(sender.start());
        assert(receiver.start());
        assert(receiver.stop());

        assert(sender.send(make_frame(3, 0x30)));
        dc::Frame received{};
        assert(!receiver.poll(received));

        assert(receiver.start());
        assert(!receiver.poll(received));
        assert(receiver.stop());
        assert(sender.send(make_frame(4, 0x40)));
        assert(receiver.start());
        assert(!receiver.poll(received));
        assert(sender.stop());
        assert(!sender.send(make_frame(5, 0x50)));
    }

    void test_lifecycle_and_destruction() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender(medium);
        assert(!sender.send(make_frame(6, 0x60)));
        assert(sender.start());
        {
            dc::FakeBus temporary(medium);
            assert(temporary.start());
        }
        assert(sender.send(make_frame(7, 0x70)));
        assert(sender.stop());
        assert(sender.state() == dc::BusState::STOPPED);
        dc::Frame received{};
        assert(!sender.poll(received));
    }
}

int main() {
    test_broadcast_and_sender_exclusion();
    test_fifo_delivery_and_stats();
    test_stopped_endpoints_are_offline();
    test_lifecycle_and_destruction();
}
