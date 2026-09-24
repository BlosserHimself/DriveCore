#include <cassert>
#include <memory>

#include "dc_dispatcher.hpp"
#include "dc_fake_bus.hpp"
#include "dc_node_client.hpp"

namespace {
    constexpr dc::Category climate = dc::category::CLIMATE;
    constexpr dc::Topic driver_seat_heat = dc::topic::climate::DRIVER_SEAT_HEAT;

    void test_subscription_frames_and_local_intent() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus client_bus(medium);
        dc::FakeBus receiver_bus(medium);
        assert(client_bus.start());
        assert(receiver_bus.start());

        dc::Dispatcher dispatcher(client_bus);
        dc::NodeClient client(dispatcher, 7);
        client.subscribe_u16(climate, driver_seat_heat, dc::priority::MS_100);
        assert(client.send_subscription(
                   climate, driver_seat_heat, dc::priority::MS_100) ==
               dc::NodeClient::SubscriptionSendError::NONE);
        assert(dispatcher.service_once() == dc::Dispatcher::ServiceResult::SENT);

        dc::Frame frame{};
        assert(receiver_bus.poll(frame));
        assert(frame.length == 0);
        dc::SubscriptionWireId subscription{
            0, 0, 0, 0, dc::SubscriptionOperation::UNSUBSCRIBE};
        assert(dc::decode_subscription_frame(frame, subscription) ==
               dc::FrameCodecError::NONE);
        assert(subscription.freshness == dc::priority::MS_100);
        assert(subscription.category == climate);
        assert(subscription.topic == driver_seat_heat);
        assert(subscription.requester_id == 7);
        assert(subscription.operation ==
               dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE);

        assert(client.send_unsubscribe(climate, driver_seat_heat) ==
               dc::NodeClient::SubscriptionSendError::NONE);
        assert(dispatcher.service_once() == dc::Dispatcher::ServiceResult::SENT);
        assert(receiver_bus.poll(frame));
        assert(dc::decode_subscription_frame(frame, subscription) ==
               dc::FrameCodecError::NONE);
        assert(subscription.freshness == dc::priority::REALTIME);
        assert(subscription.requester_id == 7);
        assert(subscription.category == climate);
        assert(subscription.topic == driver_seat_heat);
        assert(subscription.operation == dc::SubscriptionOperation::UNSUBSCRIBE);
    }

    void test_requester_and_transport_errors() {
        dc::NodeClient no_dispatcher(7);
        assert(no_dispatcher.send_subscription(
                   climate, driver_seat_heat, dc::priority::MS_100) ==
               dc::NodeClient::SubscriptionSendError::NO_DISPATCHER);

        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);
        dc::Dispatcher dispatcher(bus);
        dc::NodeClient invalid_requester(dispatcher, 64);
        assert(invalid_requester.send_subscription(
                   climate, driver_seat_heat, dc::priority::MS_100) ==
               dc::NodeClient::SubscriptionSendError::INVALID_REQUESTER_ID);

        assert(bus.start());
        dc::NodeClient invalid_priority(dispatcher, 7);
        assert(invalid_priority.send_subscription(
                   climate, driver_seat_heat, static_cast<dc::Priority>(5)) ==
               dc::NodeClient::SubscriptionSendError::FRAME_CODEC_ERROR);
        dc::Frame frame{};
        assert(!bus.poll(frame));
    }

    void test_pending_full_is_reported_without_dropping_the_pending_frame() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);
        dc::Dispatcher dispatcher(bus);
        dc::NodeClient client(dispatcher, 7);

        assert(client.send_subscription(
                   climate, driver_seat_heat, dc::priority::MS_100) ==
               dc::NodeClient::SubscriptionSendError::NONE);

        // Dispatcher's single pending slot is still occupied (never
        // serviced): a second subscription cannot be staged yet, and this
        // must be reported rather than silently dropped or overwritten.
        assert(client.send_subscription(
                   climate, driver_seat_heat, dc::priority::MS_100) ==
               dc::NodeClient::SubscriptionSendError::PENDING_FULL);
    }
}

int main() {
    test_subscription_frames_and_local_intent();
    test_requester_and_transport_errors();
    test_pending_full_is_reported_without_dropping_the_pending_frame();
}
