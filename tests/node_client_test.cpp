#include <cassert>
#include <memory>

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

        dc::NodeClient client(client_bus, 7);
        client.subscribe_u16(climate, driver_seat_heat, dc::priority::MS_100);
        assert(client.send_subscription(
                   climate, driver_seat_heat, dc::priority::MS_100) ==
               dc::NodeClient::SubscriptionSendError::NONE);

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
        dc::NodeClient no_bus(7);
        assert(no_bus.send_subscription(
                   climate, driver_seat_heat, dc::priority::MS_100) ==
               dc::NodeClient::SubscriptionSendError::NO_BUS);

        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus bus(medium);
        dc::NodeClient invalid_requester(bus, 64);
        assert(invalid_requester.send_subscription(
                   climate, driver_seat_heat, dc::priority::MS_100) ==
               dc::NodeClient::SubscriptionSendError::INVALID_REQUESTER_ID);

        assert(bus.start());
        dc::NodeClient invalid_priority(bus, 7);
        assert(invalid_priority.send_subscription(
                   climate, driver_seat_heat, static_cast<dc::Priority>(5)) ==
               dc::NodeClient::SubscriptionSendError::FRAME_CODEC_ERROR);
        dc::Frame frame{};
        assert(!bus.poll(frame));

        dc::NodeClient stopped_bus(bus, 7);
        assert(bus.stop());
        assert(stopped_bus.send_subscription(
                   climate, driver_seat_heat, dc::priority::MS_100) ==
               dc::NodeClient::SubscriptionSendError::BUS_SEND_FAILED);
    }
}

int main() {
    test_subscription_frames_and_local_intent();
    test_requester_and_transport_errors();
}
