#include <cassert>
#include <memory>

#include "dc_dispatcher.hpp"
#include "dc_fake_bus.hpp"
#include "dc_frame_codec.hpp"
#include "dc_node_client.hpp"
#include "dc_subscription_manager.hpp"
#include "dc_topics.hpp"

namespace {
    constexpr dc::Category climate = dc::category::CLIMATE;
    constexpr dc::Topic driver_seat_heat = dc::topic::climate::DRIVER_SEAT_HEAT;
    constexpr uint8_t requester_id = 7;

    bool has_no_frame(dc::FakeBus& bus) {
        dc::Frame frame{};
        return !bus.poll(frame);
    }

    void send_press(dc::FakeBus& candeck) {
        const dc::CommandWireId command_id{
            0, climate, driver_seat_heat, dc::CommandAction::PRESS, 0};
        dc::Frame command_frame{};
        assert(dc::encode_command_frame(
                   command_id, dc::FramePayload{}, command_frame) ==
               dc::FrameCodecError::NONE);
        assert(candeck.send(command_frame));
    }

    void send_subscription(dc::NodeClient& candeck_client, dc::Dispatcher& dispatcher) {
        candeck_client.subscribe_u16(climate, driver_seat_heat, dc::priority::MS_100);
        assert(candeck_client.send_subscription(
                   climate, driver_seat_heat, dc::priority::MS_100) ==
               dc::NodeClient::SubscriptionSendError::NONE);
        assert(dispatcher.service_once() == dc::Dispatcher::ServiceResult::SENT);
    }

    void publish_state(dc::FakeBus& gateway, uint8_t state) {
        const dc::DataWireId state_id{0, climate, driver_seat_heat, 0};
        dc::FramePayload state_payload{};
        state_payload.length = 1;
        state_payload.data[0] = state;
        dc::Frame state_frame{};
        assert(dc::encode_data_frame(state_id, state_payload, state_frame) ==
               dc::FrameCodecError::NONE);
        assert(gateway.send(state_frame));
    }

    void publish_state_if_subscribed(
        dc::FakeBus& gateway,
        const dc::SubscriptionManager& subscriptions,
        uint8_t state) {
        if (subscriptions.isSubscribed(climate, driver_seat_heat)) {
            publish_state(gateway, state);
        }
    }

    bool execute_press_if_matched(
        const dc::Frame& frame, uint8_t& gateway_state) {
        dc::CommandWireId command_id{
            0, 0, 0, dc::CommandAction::TRIGGER, 0};
        dc::FramePayload command_payload{};
        const bool decoded =
            dc::decode_command_frame(frame, command_id, command_payload) ==
            dc::FrameCodecError::NONE;
        if (!decoded || command_id.category != climate ||
            command_id.topic != driver_seat_heat ||
            command_id.action != dc::CommandAction::PRESS ||
            command_payload.length != 0) {
            return false;
        }

        gateway_state = static_cast<uint8_t>((gateway_state + 1) % 4);
        return true;
    }

    bool update_subscription_if_matched(
        const dc::Frame& frame, dc::SubscriptionManager& subscriptions) {
        dc::SubscriptionWireId subscription_id{
            0, 0, 0, 0, dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE};
        const dc::FrameCodecError result =
            dc::decode_subscription_frame(frame, subscription_id);
        if (result != dc::FrameCodecError::NONE ||
            subscription_id.category != climate ||
            subscription_id.topic != driver_seat_heat ||
            subscription_id.requester_id != requester_id) {
            return false;
        }

        if (subscription_id.operation ==
            dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE) {
            if (subscription_id.freshness != dc::priority::MS_100) return false;
            subscriptions.subscribe(
                subscription_id.requester_id,
                subscription_id.category,
                subscription_id.topic,
                subscription_id.freshness);
            return true;
        }
        if (subscription_id.operation == dc::SubscriptionOperation::UNSUBSCRIBE) {
            if (subscription_id.freshness != dc::priority::REALTIME) return false;
            subscriptions.unsubscribe(
                subscription_id.requester_id,
                subscription_id.category,
                subscription_id.topic);
            return true;
        }
        return false;
    }

    void receive_and_apply_command(
        dc::FakeBus& gateway, uint8_t& gateway_state) {
        dc::Frame frame{};
        assert(gateway.poll(frame));
        assert(execute_press_if_matched(frame, gateway_state));
    }

    void receive_and_apply_subscription(
        dc::FakeBus& gateway, dc::SubscriptionManager& subscriptions) {
        dc::Frame frame{};
        assert(gateway.poll(frame));
        assert(frame.length == 0);

        dc::SubscriptionWireId decoded{
            0, 0, 0, 0, dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE};
        assert(dc::decode_subscription_frame(frame, decoded) ==
               dc::FrameCodecError::NONE);
        if (decoded.operation == dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE) {
            assert(decoded.freshness == dc::priority::MS_100);
        } else {
            assert(decoded.freshness == dc::priority::REALTIME);
        }
        assert(decoded.category == climate);
        assert(decoded.topic == driver_seat_heat);
        assert(decoded.requester_id == requester_id);
        assert(decoded.operation == dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE ||
               decoded.operation == dc::SubscriptionOperation::UNSUBSCRIBE);
        assert(update_subscription_if_matched(frame, subscriptions));
    }

    void receive_state(
        dc::FakeBus& candeck, uint8_t& candeck_observed_state, uint8_t expected) {
        dc::Frame frame{};
        assert(candeck.poll(frame));

        dc::DataWireId state_id{0, 0, 0, 0};
        dc::FramePayload state_payload{};
        assert(dc::decode_data_frame(frame, state_id, state_payload) ==
               dc::FrameCodecError::NONE);
        assert(state_id.category == climate);
        assert(state_id.topic == driver_seat_heat);
        assert(state_payload.length == 1);
        candeck_observed_state = state_payload.data[0];
        assert(candeck_observed_state == expected);
    }

    void run_seat_heater_protocol_simulation() {
        auto medium = std::make_shared<dc::FakeCanMedium>();
        dc::FakeBus candeck(medium);
        dc::FakeBus gateway(medium);
        dc::Dispatcher candeck_dispatcher(candeck);
        dc::NodeClient candeck_client(candeck_dispatcher, requester_id);
        assert(candeck.start());
        assert(gateway.start());

        uint8_t gateway_state = 0;
        uint8_t candeck_observed_state = 0;
        dc::SubscriptionManager gateway_subscriptions;
        assert(gateway_state == 0);
        assert(candeck_observed_state == 0);
        assert(!gateway_subscriptions.isSubscribed(climate, driver_seat_heat));

        send_press(candeck);
        receive_and_apply_command(gateway, gateway_state);
        assert(gateway_state == 1);
        publish_state_if_subscribed(
            gateway, gateway_subscriptions, gateway_state);
        assert(has_no_frame(candeck));

        send_subscription(candeck_client, candeck_dispatcher);
        receive_and_apply_subscription(gateway, gateway_subscriptions);
        assert(gateway_subscriptions.isSubscribed(climate, driver_seat_heat));
        assert(has_no_frame(candeck));

        send_press(candeck);
        receive_and_apply_command(gateway, gateway_state);
        assert(gateway_state == 2);
        assert(gateway_subscriptions.isSubscribed(climate, driver_seat_heat));
        publish_state_if_subscribed(
            gateway, gateway_subscriptions, gateway_state);
        receive_state(candeck, candeck_observed_state, 2);
        assert(has_no_frame(gateway));

        send_press(candeck);
        receive_and_apply_command(gateway, gateway_state);
        assert(gateway_state == 3);
        publish_state_if_subscribed(
            gateway, gateway_subscriptions, gateway_state);
        receive_state(candeck, candeck_observed_state, 3);
        assert(has_no_frame(gateway));

        assert(candeck_client.send_unsubscribe(climate, driver_seat_heat) ==
               dc::NodeClient::SubscriptionSendError::NONE);
        assert(candeck_dispatcher.service_once() ==
               dc::Dispatcher::ServiceResult::SENT);
        receive_and_apply_subscription(gateway, gateway_subscriptions);
        assert(!gateway_subscriptions.isSubscribed(climate, driver_seat_heat));
        assert(has_no_frame(candeck));

        send_press(candeck);
        receive_and_apply_command(gateway, gateway_state);
        assert(gateway_state == 0);
        assert(!gateway_subscriptions.isSubscribed(climate, driver_seat_heat));
        publish_state_if_subscribed(
            gateway, gateway_subscriptions, gateway_state);
        assert(has_no_frame(candeck));
        assert(candeck_observed_state == 3);
    }
}

int main() {
    run_seat_heater_protocol_simulation();
}
