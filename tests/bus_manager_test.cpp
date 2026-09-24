#include <cassert>
#include <memory>

#include "dc_bus_manager.hpp"
#include "dc_dispatcher.hpp"
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

    void test_attach_transmitter_succeeds_and_dispatcher_accessible() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus peer(medium);
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));

        assert(manager.attach_transmitter(bus) ==
               dc::BusManager::AttachTransmitterError::NONE);

        dc::Dispatcher* dispatcher = manager.dispatcher(bus);
        assert(dispatcher != nullptr);

        assert(peer.start());
        assert(bus.start());
        assert(dispatcher->submit(make_frame(0x123, 0x5A)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);

        assert(manager.service_transmitters_once());

        dc::Frame received{};
        assert(peer.poll(received));
        assert(received.id == 0x123);
        assert(received.length == 1);
        assert(received.data[0] == 0x5A);
    }

    void test_attach_transmitter_rejects_unowned_bus() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        manager.add(std::make_unique<dc::FakeBus>(medium));

        dc::FakeBus unowned(medium);
        assert(manager.attach_transmitter(unowned) ==
               dc::BusManager::AttachTransmitterError::UNKNOWN_BUS);
    }

    void test_attach_transmitter_rejects_repeated_attachment() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));

        assert(manager.attach_transmitter(bus) ==
               dc::BusManager::AttachTransmitterError::NONE);
        assert(manager.attach_transmitter(bus) ==
               dc::BusManager::AttachTransmitterError::ALREADY_ATTACHED);
    }

    void test_dispatcher_accessor_reports_absence_without_attachment() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));

        assert(manager.dispatcher(bus) == nullptr);
    }

    void test_dispatcher_accessor_reports_absence_for_unknown_bus() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        manager.add(std::make_unique<dc::FakeBus>(medium));

        dc::FakeBus unowned(medium);
        assert(manager.dispatcher(unowned) == nullptr);
    }

    void test_rx_tx_attachment_independence() {
        dc::BusManager manager;
        dc::Frame received{};

        // TX-only: works without any receiver attached.
        auto tx_medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus tx_peer(tx_medium);
        dc::IBus& tx_only = manager.add(std::make_unique<dc::FakeBus>(tx_medium));
        assert(manager.attach_transmitter(tx_only) ==
               dc::BusManager::AttachTransmitterError::NONE);
        assert(manager.dispatcher(tx_only) != nullptr);
        assert(tx_peer.start());
        assert(tx_only.start());
        assert(manager.dispatcher(tx_only)->submit(make_frame(1, 0x10)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);
        assert(manager.service_transmitters_once());
        assert(tx_peer.poll(received));
        assert(received.id == 1);

        // RX-only: works without any transmitter attached.
        auto rx_medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus rx_peer(rx_medium);
        dc::IBus& rx_only = manager.add(std::make_unique<dc::FakeBus>(rx_medium));
        RecordingSink rx_only_sink;
        assert(manager.attach_receiver(rx_only, rx_only_sink) ==
               dc::BusManager::AttachReceiverError::NONE);
        assert(manager.dispatcher(rx_only) == nullptr);
        assert(rx_peer.start());
        assert(rx_only.start());
        assert(rx_peer.send(make_frame(2, 0x20)));
        assert(manager.poll_receivers_once());
        assert(rx_only_sink.received_count == 1);
        assert(rx_only_sink.received.id == 2);

        // RX+TX: both attached to the same bus, operating independently.
        auto both_medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus both_peer(both_medium);
        dc::IBus& both = manager.add(std::make_unique<dc::FakeBus>(both_medium));
        RecordingSink both_sink;
        assert(manager.attach_receiver(both, both_sink) ==
               dc::BusManager::AttachReceiverError::NONE);
        assert(manager.attach_transmitter(both) ==
               dc::BusManager::AttachTransmitterError::NONE);
        assert(manager.dispatcher(both) != nullptr);
        assert(both_peer.start());
        assert(both.start());

        assert(both_peer.send(make_frame(3, 0x30)));
        assert(manager.dispatcher(both)->submit(make_frame(4, 0x40)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);

        assert(manager.poll_receivers_once());
        assert(both_sink.received_count == 1);
        assert(both_sink.received.id == 3);

        assert(manager.service_transmitters_once());
        assert(both_peer.poll(received));
        assert(received.id == 4);
    }

    void test_service_transmitters_once_with_no_transmitters_returns_false() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
        assert(bus.start());

        assert(!manager.service_transmitters_once());
        assert(bus.stats().tx_frames == 0);
    }

    void test_service_transmitters_once_with_nothing_pending_returns_false() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
        assert(manager.attach_transmitter(bus) ==
               dc::BusManager::AttachTransmitterError::NONE);
        assert(bus.start());

        assert(!manager.service_transmitters_once());
        assert(bus.stats().tx_frames == 0);
    }

    void test_service_transmitters_once_gives_each_dispatcher_one_opportunity() {
        auto medium_a = std::make_shared<dc::FakeCanMedium>();
        auto medium_b = std::make_shared<dc::FakeCanMedium>();
        auto medium_c = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus peer_a(medium_a);
        dc::FakeBus peer_b(medium_b);
        dc::FakeBus peer_c(medium_c);

        dc::BusManager manager;
        dc::IBus& bus_a = manager.add(std::make_unique<dc::FakeBus>(medium_a));
        dc::IBus& bus_b = manager.add(std::make_unique<dc::FakeBus>(medium_b));
        dc::IBus& bus_c = manager.add(std::make_unique<dc::FakeBus>(medium_c));

        assert(manager.attach_transmitter(bus_a) ==
               dc::BusManager::AttachTransmitterError::NONE);
        assert(manager.attach_transmitter(bus_b) ==
               dc::BusManager::AttachTransmitterError::NONE);
        assert(manager.attach_transmitter(bus_c) ==
               dc::BusManager::AttachTransmitterError::NONE);

        assert(peer_a.start());
        assert(peer_b.start());
        assert(peer_c.start());
        assert(bus_a.start());
        assert(bus_b.start());
        assert(bus_c.start());

        assert(manager.dispatcher(bus_a)->submit(make_frame(0xA1, 0)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);
        assert(manager.dispatcher(bus_b)->submit(make_frame(0xB1, 0)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);
        assert(manager.dispatcher(bus_c)->submit(make_frame(0xC1, 0)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);

        assert(manager.service_transmitters_once());

        dc::Frame received{};
        assert(peer_a.poll(received));
        assert(received.id == 0xA1);
        assert(peer_b.poll(received));
        assert(received.id == 0xB1);
        assert(peer_c.poll(received));
        assert(received.id == 0xC1);

        // Each dispatcher's pending slot is now empty: a further submit
        // succeeds instead of reporting PENDING_FULL, proving all three were
        // actually serviced in the single manager call above, not just the
        // first (no short-circuit after bus_a's success).
        assert(manager.dispatcher(bus_a)->submit(make_frame(0xA2, 0)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);
        assert(manager.dispatcher(bus_b)->submit(make_frame(0xB2, 0)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);
        assert(manager.dispatcher(bus_c)->submit(make_frame(0xC2, 0)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);
    }

    void test_service_transmitters_once_failed_transmitter_does_not_block_others_and_retries_later() {
        auto medium_a = std::make_shared<dc::FakeCanMedium>();
        auto medium_b = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus peer_a(medium_a);
        dc::FakeBus peer_b(medium_b);

        dc::BusManager manager;
        dc::IBus& bus_a = manager.add(std::make_unique<dc::FakeBus>(medium_a));
        dc::IBus& bus_b = manager.add(std::make_unique<dc::FakeBus>(medium_b));

        assert(manager.attach_transmitter(bus_a) ==
               dc::BusManager::AttachTransmitterError::NONE);
        assert(manager.attach_transmitter(bus_b) ==
               dc::BusManager::AttachTransmitterError::NONE);

        assert(peer_a.start());
        assert(peer_b.start());
        assert(bus_b.start());
        // bus_a is intentionally never started: FakeBus::send() fails while
        // its endpoint is not RUNNING.

        assert(manager.dispatcher(bus_a)->submit(make_frame(0x77, 0)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);
        assert(manager.dispatcher(bus_b)->submit(make_frame(0x88, 0)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);

        // bus_a's send fails, but bus_b is still serviced in the same call.
        assert(manager.service_transmitters_once());
        dc::Frame received{};
        assert(peer_b.poll(received));
        assert(received.id == 0x88);
        assert(bus_a.stats().tx_frames == 0);

        // bus_a's frame remained pending: a further submit is rejected as
        // still occupied rather than silently accepted.
        assert(manager.dispatcher(bus_a)->submit(make_frame(0x99, 0)) ==
               dc::Dispatcher::SubmitResult::PENDING_FULL);

        // Once bus_a can send, a later manager service call succeeds with
        // the originally-pending frame.
        assert(bus_a.start());
        assert(manager.service_transmitters_once());
        assert(peer_a.poll(received));
        assert(received.id == 0x77);
    }

    void test_service_transmitters_once_makes_at_most_one_attempt_per_dispatcher() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus peer(medium);
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));
        assert(manager.attach_transmitter(bus) ==
               dc::BusManager::AttachTransmitterError::NONE);

        assert(peer.start());
        assert(bus.start());
        assert(manager.dispatcher(bus)->submit(make_frame(1, 0)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);

        const uint32_t before = bus.stats().tx_frames;
        assert(manager.service_transmitters_once());
        assert(bus.stats().tx_frames == before + 1);

        // Nothing left pending: a further manager service call must not
        // attempt another send.
        assert(!manager.service_transmitters_once());
        assert(bus.stats().tx_frames == before + 1);
    }

    void test_service_transmitters_once_does_not_poll_receivers() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus peer(medium);
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));

        RecordingSink sink;
        assert(manager.attach_receiver(bus, sink) ==
               dc::BusManager::AttachReceiverError::NONE);
        assert(manager.attach_transmitter(bus) ==
               dc::BusManager::AttachTransmitterError::NONE);

        assert(peer.start());
        assert(bus.start());

        // A frame is waiting to be received...
        assert(peer.send(make_frame(1, 0x10)));
        // ...and a frame is pending to be sent.
        assert(manager.dispatcher(bus)->submit(make_frame(2, 0x20)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);

        assert(manager.service_transmitters_once());
        assert(sink.received_count == 0); // servicing TX did not poll RX

        dc::Frame received{};
        assert(peer.poll(received));
        assert(received.id == 2); // the transmitted frame did arrive
    }

    void test_poll_receivers_once_does_not_service_transmitters() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus peer(medium);
        dc::BusManager manager;
        dc::IBus& bus = manager.add(std::make_unique<dc::FakeBus>(medium));

        RecordingSink sink;
        assert(manager.attach_receiver(bus, sink) ==
               dc::BusManager::AttachReceiverError::NONE);
        assert(manager.attach_transmitter(bus) ==
               dc::BusManager::AttachTransmitterError::NONE);

        assert(peer.start());
        assert(bus.start());

        assert(peer.send(make_frame(1, 0x10)));
        assert(manager.dispatcher(bus)->submit(make_frame(3, 0)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);

        assert(manager.poll_receivers_once());
        assert(sink.received_count == 1);
        assert(sink.received.id == 1);

        // Pending TX frame was untouched: submit() still reports occupied.
        assert(manager.dispatcher(bus)->submit(make_frame(4, 0)) ==
               dc::Dispatcher::SubmitResult::PENDING_FULL);

        dc::Frame received{};
        assert(!peer.poll(received)); // nothing was physically transmitted
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
    test_attach_transmitter_succeeds_and_dispatcher_accessible();
    test_attach_transmitter_rejects_unowned_bus();
    test_attach_transmitter_rejects_repeated_attachment();
    test_dispatcher_accessor_reports_absence_without_attachment();
    test_dispatcher_accessor_reports_absence_for_unknown_bus();
    test_rx_tx_attachment_independence();
    test_service_transmitters_once_with_no_transmitters_returns_false();
    test_service_transmitters_once_with_nothing_pending_returns_false();
    test_service_transmitters_once_gives_each_dispatcher_one_opportunity();
    test_service_transmitters_once_failed_transmitter_does_not_block_others_and_retries_later();
    test_service_transmitters_once_makes_at_most_one_attempt_per_dispatcher();
    test_service_transmitters_once_does_not_poll_receivers();
    test_poll_receivers_once_does_not_service_transmitters();
}
