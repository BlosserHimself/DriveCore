#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "dc_wire_id.hpp"

namespace {
    void test_data_round_trip_and_boundaries() {
        for (const uint8_t arbitration_class : {uint8_t{0}, uint8_t{7}}) {
            for (const uint8_t category : {uint8_t{0}, uint8_t{0xFF}}) {
                for (const uint8_t topic : {uint8_t{0}, uint8_t{0xFF}}) {
                    const dc::DataWireId input{
                        arbitration_class, category, topic, 0};
                    uint32_t id = 0;
                    assert(dc::encode_data_id(input, id) == dc::WireIdError::NONE);
                    assert(id <= dc::wire_id::MAX_EXTENDED_ID);
                    assert((id & 0xFFu) == 0u);

                    dc::DataWireId output{7, 0xAA, 0xAA, 0xAA};
                    assert(dc::decode_data_id(id, output) == dc::WireIdError::NONE);
                    assert(output.arbitration_class == input.arbitration_class);
                    assert(output.category == input.category);
                    assert(output.topic == input.topic);
                    assert(output.extension == 0);
                }
            }
        }
    }

    void test_data_validation() {
        uint32_t id = 123;
        assert(dc::encode_data_id({8, 0, 0, 0}, id) ==
               dc::WireIdError::VALUE_OUT_OF_RANGE);
        assert(dc::encode_data_id({0, 0, 0, 1}, id) ==
               dc::WireIdError::NONZERO_DATA_EXTENSION);

        const dc::SubscriptionWireId subscription{
            dc::priority::REALTIME, 0, 0, 0,
            dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE};
        assert(dc::encode_subscription_id(subscription, id) == dc::WireIdError::NONE);

        dc::DataWireId data{0, 0, 0, 0};
        assert(dc::decode_data_id(id, data) == dc::WireIdError::WRONG_KIND);
        assert(dc::decode_data_id(0x20000000u, data) ==
               dc::WireIdError::VALUE_OUT_OF_RANGE);

        assert(dc::decode_data_id(0x00000001u, data) ==
               dc::WireIdError::NONZERO_DATA_EXTENSION);
    }

    void test_subscription_round_trip_and_valid_freshness() {
        for (const uint8_t freshness : {uint8_t{0}, uint8_t{1}, uint8_t{2},
                                                      uint8_t{3}, uint8_t{4}}) {
            for (const uint8_t requester_id : {uint8_t{0}, uint8_t{63}}) {
                for (const uint8_t category : {uint8_t{0}, uint8_t{0xFF}}) {
                    for (const uint8_t topic : {uint8_t{0}, uint8_t{0xFF}}) {
                        for (const auto operation : {
                                 dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE,
                                 dc::SubscriptionOperation::UNSUBSCRIBE}) {
                            const dc::SubscriptionWireId input{
                                freshness, category, topic, requester_id, operation};
                            uint32_t id = 0;
                            assert(dc::encode_subscription_id(input, id) ==
                                   dc::WireIdError::NONE);
                            assert(id <= dc::wire_id::MAX_EXTENDED_ID);

                            dc::SubscriptionWireId output{
                                7, 0, 0, 63,
                                dc::SubscriptionOperation::UNSUBSCRIBE};
                            assert(dc::decode_subscription_id(id, output) ==
                                   dc::WireIdError::NONE);
                            assert(output.freshness == input.freshness);
                            assert(output.category == input.category);
                            assert(output.topic == input.topic);
                            assert(output.requester_id == input.requester_id);
                            assert(output.operation == input.operation);
                        }
                    }
                }
            }
        }
    }

    void test_subscription_validation() {
        uint32_t id = 0;
        const auto subscribe = dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE;

        assert(dc::encode_subscription_id({0, 0, 0, 64, subscribe}, id) ==
               dc::WireIdError::VALUE_OUT_OF_RANGE);
        assert(dc::encode_subscription_id({5, 0, 0, 0, subscribe}, id) ==
               dc::WireIdError::RESERVED_FRESHNESS);
        assert(dc::encode_subscription_id({6, 0, 0, 0, subscribe}, id) ==
               dc::WireIdError::RESERVED_FRESHNESS);
        assert(dc::encode_subscription_id({7, 0, 0, 0, subscribe}, id) ==
               dc::WireIdError::RESERVED_FRESHNESS);
        assert(dc::encode_subscription_id(
                   {0, 0, 0, 0, static_cast<dc::SubscriptionOperation>(2)}, id) ==
               dc::WireIdError::RESERVED_OPERATION);
        assert(dc::encode_subscription_id(
                   {0, 0, 0, 0, static_cast<dc::SubscriptionOperation>(3)}, id) ==
               dc::WireIdError::RESERVED_OPERATION);

        dc::SubscriptionWireId output{
            0, 0, 0, 0, dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE};
        assert(dc::decode_subscription_id(0x15000000u, output) ==
               dc::WireIdError::RESERVED_FRESHNESS);
        assert(dc::decode_subscription_id(0x01000002u, output) ==
               dc::WireIdError::RESERVED_OPERATION);

        const dc::DataWireId data{0, 0, 0, 0};
        assert(dc::encode_data_id(data, id) == dc::WireIdError::NONE);
        assert(dc::decode_subscription_id(id, output) == dc::WireIdError::WRONG_KIND);
    }

    void test_command_round_trip_and_boundaries() {
        const auto actions = {
            dc::CommandAction::TRIGGER,
            dc::CommandAction::PRESS,
            dc::CommandAction::HOLD,
            dc::CommandAction::BUTTON_DOWN,
            dc::CommandAction::BUTTON_UP,
            dc::CommandAction::SET,
            dc::CommandAction::INCREMENT,
            dc::CommandAction::DECREMENT,
            dc::CommandAction::TOGGLE,
        };

        for (const uint8_t arbitration_class : {uint8_t{0}, uint8_t{7}}) {
            for (const uint8_t category : {uint8_t{0}, uint8_t{0xFF}}) {
                for (const uint8_t topic : {uint8_t{0}, uint8_t{0xFF}}) {
                    for (const auto action : actions) {
                        const dc::CommandWireId input{
                            arbitration_class, category, topic, action, 0};
                        uint32_t id = 0;
                        assert(dc::encode_command_id(input, id) ==
                               dc::WireIdError::NONE);
                        assert(id <= dc::wire_id::MAX_EXTENDED_ID);
                        assert((id & 0x0Fu) == 0u);

                        dc::CommandWireId output{
                            7, 0xAA, 0xAA, dc::CommandAction::TOGGLE, 0xAA};
                        assert(dc::decode_command_id(id, output) ==
                               dc::WireIdError::NONE);
                        assert(output.arbitration_class == input.arbitration_class);
                        assert(output.category == input.category);
                        assert(output.topic == input.topic);
                        assert(output.action == input.action);
                        assert(output.extension == 0);
                    }
                }
            }
        }
    }

    void test_command_validation() {
        uint32_t id = 0;
        const auto action = dc::CommandAction::TRIGGER;

        assert(dc::encode_command_id({8, 0, 0, action, 0}, id) ==
               dc::WireIdError::VALUE_OUT_OF_RANGE);
        assert(dc::encode_command_id(
                   {0, 0, 0, static_cast<dc::CommandAction>(9), 0}, id) ==
               dc::WireIdError::RESERVED_COMMAND_ACTION);
        assert(dc::encode_command_id(
                   {0, 0, 0, static_cast<dc::CommandAction>(15), 0}, id) ==
               dc::WireIdError::RESERVED_COMMAND_ACTION);
        assert(dc::encode_command_id({0, 0, 0, action, 1}, id) ==
               dc::WireIdError::NONZERO_COMMAND_EXTENSION);

        dc::CommandWireId output{
            0, 0, 0, dc::CommandAction::TRIGGER, 0};
        assert(dc::decode_command_id(0x02000090u, output) ==
               dc::WireIdError::RESERVED_COMMAND_ACTION);
        assert(dc::decode_command_id(0x02000011u, output) ==
               dc::WireIdError::NONZERO_COMMAND_EXTENSION);
        assert(dc::decode_command_id(0x20000000u, output) ==
               dc::WireIdError::VALUE_OUT_OF_RANGE);

        const dc::DataWireId data{0, 0, 0, 0};
        assert(dc::encode_data_id(data, id) == dc::WireIdError::NONE);
        assert(dc::decode_command_id(id, output) == dc::WireIdError::WRONG_KIND);

        const dc::SubscriptionWireId subscription{
            dc::priority::REALTIME, 0, 0, 0,
            dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE};
        assert(dc::encode_subscription_id(subscription, id) ==
               dc::WireIdError::NONE);
        assert(dc::decode_command_id(id, output) == dc::WireIdError::WRONG_KIND);
    }
}

int main() {
    test_data_round_trip_and_boundaries();
    test_data_validation();
    test_subscription_round_trip_and_valid_freshness();
    test_subscription_validation();
    test_command_round_trip_and_boundaries();
    test_command_validation();
}
