#include <cassert>
#include <memory>

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

    void test_construction_does_not_mutate_bus_lifecycle() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);

        dc::Dispatcher dispatcher(bus);

        assert(bus.state() == dc::BusState::STOPPED);
    }

    void test_submit_does_not_physically_send() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);
        assert(bus.start());

        dc::Dispatcher dispatcher(bus);
        assert(dispatcher.submit(make_frame(1, 0x10)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);

        assert(bus.stats().tx_frames == 0);
    }

    void test_service_once_sends_one_submitted_frame_exactly() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);
        dc::FakeBus peer(medium);
        assert(bus.start());
        assert(peer.start());

        dc::Dispatcher dispatcher(bus);
        const dc::Frame sent = make_frame(0x123, 0x5A);
        assert(dispatcher.submit(sent) == dc::Dispatcher::SubmitResult::ACCEPTED);

        assert(dispatcher.service_once() == dc::Dispatcher::ServiceResult::SENT);
        assert(bus.stats().tx_frames == 1);

        dc::Frame received{};
        assert(peer.poll(received));
        assert(received.id == sent.id);
        assert(received.length == sent.length);
        assert(received.data[0] == sent.data[0]);
    }

    void test_service_once_with_nothing_pending_does_not_send() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);
        assert(bus.start());

        dc::Dispatcher dispatcher(bus);
        assert(dispatcher.service_once() ==
               dc::Dispatcher::ServiceResult::NOTHING_PENDING);
        assert(bus.stats().tx_frames == 0);
    }

    void test_second_submit_while_pending_reports_backpressure() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);
        dc::FakeBus peer(medium);
        assert(bus.start());
        assert(peer.start());

        dc::Dispatcher dispatcher(bus);
        assert(dispatcher.submit(make_frame(1, 0x10)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);
        assert(dispatcher.submit(make_frame(2, 0x20)) ==
               dc::Dispatcher::SubmitResult::PENDING_FULL);

        assert(dispatcher.service_once() == dc::Dispatcher::ServiceResult::SENT);

        dc::Frame received{};
        assert(peer.poll(received));
        assert(received.id == 1); // first frame, not overwritten by the second
    }

    void test_successful_send_clears_pending_frame() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);
        assert(bus.start());

        dc::Dispatcher dispatcher(bus);
        assert(dispatcher.submit(make_frame(1, 0x10)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);
        assert(dispatcher.service_once() == dc::Dispatcher::ServiceResult::SENT);

        assert(dispatcher.submit(make_frame(2, 0x20)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);
    }

    void test_failed_send_preserves_pending_frame_until_bus_recovers() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);
        dc::FakeBus peer(medium);
        assert(peer.start());
        // `bus` is intentionally never started: FakeBus::send() fails while
        // its endpoint is not RUNNING.

        dc::Dispatcher dispatcher(bus);
        assert(dispatcher.submit(make_frame(0x77, 0xAA)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);

        assert(dispatcher.service_once() ==
               dc::Dispatcher::ServiceResult::SEND_FAILED);
        assert(bus.stats().tx_frames == 0);

        // Pending frame was preserved, not dropped: a further submit is
        // rejected as still-occupied rather than silently accepted.
        assert(dispatcher.submit(make_frame(0x99, 0xBB)) ==
               dc::Dispatcher::SubmitResult::PENDING_FULL);

        assert(bus.start());
        assert(dispatcher.service_once() == dc::Dispatcher::ServiceResult::SENT);
        assert(bus.stats().tx_frames == 1);

        dc::Frame received{};
        assert(peer.poll(received));
        assert(received.id == 0x77); // the originally-pending frame, unchanged
    }

    void test_service_once_performs_at_most_one_send_attempt() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);
        assert(bus.start());

        dc::Dispatcher dispatcher(bus);
        assert(dispatcher.submit(make_frame(1, 0x10)) ==
               dc::Dispatcher::SubmitResult::ACCEPTED);

        const uint32_t before = bus.stats().tx_frames;
        assert(dispatcher.service_once() == dc::Dispatcher::ServiceResult::SENT);
        assert(bus.stats().tx_frames == before + 1);

        // Nothing left pending: a further call must not attempt another send.
        assert(dispatcher.service_once() ==
               dc::Dispatcher::ServiceResult::NOTHING_PENDING);
        assert(bus.stats().tx_frames == before + 1);
    }
}

int main() {
    test_construction_does_not_mutate_bus_lifecycle();
    test_submit_does_not_physically_send();
    test_service_once_sends_one_submitted_frame_exactly();
    test_service_once_with_nothing_pending_does_not_send();
    test_second_submit_while_pending_reports_backpressure();
    test_successful_send_clears_pending_frame();
    test_failed_send_preserves_pending_frame_until_bus_recovers();
    test_service_once_performs_at_most_one_send_attempt();
}
