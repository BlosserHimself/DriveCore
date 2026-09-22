#include <cassert>
#include <memory>

#include "dc_bus_manager.hpp"
#include "dc_fake_bus.hpp"

namespace {
    dc::Frame make_frame(uint32_t id, uint8_t value) {
        dc::Frame frame{};
        frame.id = id;
        frame.data[0] = value;
        frame.length = 1;
        return frame;
    }

    class RecordingSink final : public dc::IFrameSink {
        public:
            void on_frame(const dc::Frame& frame) override {
                ++received_count;
                received = frame;
            }

            uint32_t received_count = 0;
            dc::Frame received{};
    };

    void test_add_returns_owned_bus_reference() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;

        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
        assert(&bus == manager.get(dc::BusType::DRIVECORE));
    }

    void test_attach_receiver_does_not_affect_pump_delivery() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender(medium);
        dc::BusManager manager;

        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
        RecordingSink sink;
        assert(manager.attach_receiver(bus, sink) ==
               dc::BusManager::AttachReceiverError::NONE);

        assert(sender.start());
        assert(bus.start());
        assert(sender.send(make_frame(0x123, 0x5A)));

        manager.pump([&sink](dc::IBus&, const dc::Frame& frame) {
            assert(sink.received_count == 0);
            assert(frame.id == 0x123);
        });
        assert(sink.received_count == 0);
    }

    void test_attach_receiver_rejects_unowned_bus() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        manager.add(std::make_unique<dc::FakeBus>(medium));

        dc::FakeBus unowned(medium);
        RecordingSink sink;
        assert(manager.attach_receiver(unowned, sink) ==
               dc::BusManager::AttachReceiverError::UNKNOWN_BUS);
    }

    void test_attach_receiver_rejects_repeated_attachment() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));

        RecordingSink first_sink;
        RecordingSink second_sink;
        assert(manager.attach_receiver(bus, first_sink) ==
               dc::BusManager::AttachReceiverError::NONE);
        assert(manager.attach_receiver(bus, second_sink) ==
               dc::BusManager::AttachReceiverError::ALREADY_ATTACHED);
    }

    void test_start_stop_and_snapshot_unaffected() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
        RecordingSink sink;
        assert(manager.attach_receiver(bus, sink) ==
               dc::BusManager::AttachReceiverError::NONE);

        assert(manager.start_all());
        assert(bus.state() == dc::BusState::RUNNING);
        assert(manager.start(dc::BusType::DRIVECORE));
        assert(!manager.start(dc::BusType::OEM));

        manager.stop(dc::BusType::DRIVECORE);
        assert(bus.state() == dc::BusState::STOPPED);

        assert(manager.start_all());
        manager.stop_all();
        assert(bus.state() == dc::BusState::STOPPED);

        const auto snapshot = manager.snapshot();
        assert(snapshot.size() == 1);
        assert(snapshot[0].type == dc::BusType::DRIVECORE);
        assert(snapshot[0].state == dc::BusState::STOPPED);
    }

    void test_pump_behavior_preserved_with_entry_storage() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender(medium);
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));

        assert(sender.start());
        assert(bus.start());
        assert(sender.send(make_frame(1, 0x10)));
        assert(sender.send(make_frame(2, 0x20)));

        uint32_t pumped_count = 0;
        manager.pump([&pumped_count](dc::IBus&, const dc::Frame& frame) {
            assert(frame.id == pumped_count + 1);
            ++pumped_count;
        });
        assert(pumped_count == 2);

        pumped_count = 0;
        manager.pump([&pumped_count](dc::IBus&, const dc::Frame&) {
            ++pumped_count;
        });
        assert(pumped_count == 0);
    }

    void test_manager_destruction_with_attached_monitor_is_safe() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        RecordingSink sink;
        {
            dc::BusManager manager;
            dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
            assert(manager.attach_receiver(bus, sink) ==
                   dc::BusManager::AttachReceiverError::NONE);
            assert(bus.start());
        }
        assert(sink.received_count == 0);
    }

    void test_poll_receivers_once_with_no_monitors_returns_false() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
        assert(bus.start());

        assert(!manager.poll_receivers_once());
    }

    void test_poll_receivers_once_with_no_frame_returns_false() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
        RecordingSink sink;
        assert(manager.attach_receiver(bus, sink) ==
               dc::BusManager::AttachReceiverError::NONE);
        assert(bus.start());

        assert(!manager.poll_receivers_once());
        assert(sink.received_count == 0);
    }

    void test_poll_receivers_once_delivers_single_frame() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender(medium);
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
        RecordingSink sink;
        assert(manager.attach_receiver(bus, sink) ==
               dc::BusManager::AttachReceiverError::NONE);

        assert(sender.start());
        assert(bus.start());
        assert(sender.send(make_frame(0x123, 0x5A)));

        assert(manager.poll_receivers_once());
        assert(sink.received_count == 1);
        assert(sink.received.id == 0x123);
    }

    void test_poll_receivers_once_processes_at_most_one_queued_frame() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender(medium);
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
        RecordingSink sink;
        assert(manager.attach_receiver(bus, sink) ==
               dc::BusManager::AttachReceiverError::NONE);

        assert(sender.start());
        assert(bus.start());
        assert(sender.send(make_frame(1, 0x10)));
        assert(sender.send(make_frame(2, 0x20)));

        assert(manager.poll_receivers_once());
        assert(sink.received_count == 1);
        assert(sink.received.id == 1);

        assert(manager.poll_receivers_once());
        assert(sink.received_count == 2);
        assert(sink.received.id == 2);

        assert(!manager.poll_receivers_once());
        assert(sink.received_count == 2);
    }

    void test_poll_receivers_once_gives_each_monitor_one_opportunity() {
        auto medium_a = std::make_shared<dc::FakeCanMedium>();
        auto medium_b = std::make_shared<dc::FakeCanMedium>();
        auto medium_c = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender_a(medium_a);
        dc::FakeBus sender_b(medium_b);
        dc::FakeBus sender_c(medium_c);
        dc::BusManager manager;
        dc::IBus& bus_a = manager.add(std::make_unique<dc::FakeBus>(medium_a));
        dc::IBus& bus_b = manager.add(std::make_unique<dc::FakeBus>(medium_b));
        dc::IBus& bus_c = manager.add(std::make_unique<dc::FakeBus>(medium_c));
        RecordingSink sink_a;
        RecordingSink sink_b;
        RecordingSink sink_c;
        assert(manager.attach_receiver(bus_a, sink_a) ==
               dc::BusManager::AttachReceiverError::NONE);
        assert(manager.attach_receiver(bus_b, sink_b) ==
               dc::BusManager::AttachReceiverError::NONE);
        assert(manager.attach_receiver(bus_c, sink_c) ==
               dc::BusManager::AttachReceiverError::NONE);

        assert(sender_a.start());
        assert(sender_b.start());
        assert(sender_c.start());
        assert(bus_a.start());
        assert(bus_b.start());
        assert(bus_c.start());

        // bus_a is deliberately made busy with two queued frames, bus_b gets
        // one, bus_c gets none, proving a busy first bus is not drained
        // before the later buses receive their single opportunity.
        assert(sender_a.send(make_frame(0xA1, 0)));
        assert(sender_a.send(make_frame(0xA2, 0)));
        assert(sender_b.send(make_frame(0xB1, 0)));

        assert(manager.poll_receivers_once());
        assert(sink_a.received_count == 1);
        assert(sink_a.received.id == 0xA1);
        assert(sink_b.received_count == 1);
        assert(sink_b.received.id == 0xB1);
        assert(sink_c.received_count == 0);

        assert(manager.poll_receivers_once());
        assert(sink_a.received_count == 2);
        assert(sink_a.received.id == 0xA2);
        assert(sink_b.received_count == 1);
        assert(sink_c.received_count == 0);

        assert(!manager.poll_receivers_once());
        assert(sink_a.received_count == 2);
        assert(sink_b.received_count == 1);
        assert(sink_c.received_count == 0);
    }

    void test_poll_receivers_once_true_if_any_monitor_receives() {
        auto medium_a = std::make_shared<dc::FakeCanMedium>();
        auto medium_b = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender_a(medium_a);
        dc::FakeBus sender_b(medium_b);
        dc::BusManager manager;
        dc::IBus& bus_a = manager.add(std::make_unique<dc::FakeBus>(medium_a));
        dc::IBus& bus_b = manager.add(std::make_unique<dc::FakeBus>(medium_b));
        RecordingSink sink_a;
        RecordingSink sink_b;
        assert(manager.attach_receiver(bus_a, sink_a) ==
               dc::BusManager::AttachReceiverError::NONE);
        assert(manager.attach_receiver(bus_b, sink_b) ==
               dc::BusManager::AttachReceiverError::NONE);

        assert(sender_a.start());
        assert(sender_b.start());
        assert(bus_a.start());
        assert(bus_b.start());
        assert(sender_a.send(make_frame(1, 0x10)));

        assert(manager.poll_receivers_once());
        assert(sink_a.received_count == 1);
        assert(sink_b.received_count == 0);
    }

    void test_poll_receivers_once_skips_buses_without_monitor() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus sender(medium);
        dc::BusManager manager;
        dc::IBus& monitored_bus =
            manager.add(std::make_unique<dc::FakeBus>(medium));
        dc::IBus& unmonitored_bus =
            manager.add(std::make_unique<dc::FakeBus>(medium));
        RecordingSink sink;
        assert(manager.attach_receiver(monitored_bus, sink) ==
               dc::BusManager::AttachReceiverError::NONE);

        assert(sender.start());
        assert(monitored_bus.start());
        assert(unmonitored_bus.start());
        assert(sender.send(make_frame(1, 0x10)));

        assert(manager.poll_receivers_once());
        assert(sink.received_count == 1);

        dc::Frame frame{};
        assert(unmonitored_bus.poll(frame));
        assert(frame.id == 1);
    }
}

int main() {
    test_add_returns_owned_bus_reference();
    test_attach_receiver_does_not_affect_pump_delivery();
    test_attach_receiver_rejects_unowned_bus();
    test_attach_receiver_rejects_repeated_attachment();
    test_start_stop_and_snapshot_unaffected();
    test_pump_behavior_preserved_with_entry_storage();
    test_manager_destruction_with_attached_monitor_is_safe();
    test_poll_receivers_once_with_no_monitors_returns_false();
    test_poll_receivers_once_with_no_frame_returns_false();
    test_poll_receivers_once_delivers_single_frame();
    test_poll_receivers_once_processes_at_most_one_queued_frame();
    test_poll_receivers_once_gives_each_monitor_one_opportunity();
    test_poll_receivers_once_true_if_any_monitor_receives();
    test_poll_receivers_once_skips_buses_without_monitor();
}
