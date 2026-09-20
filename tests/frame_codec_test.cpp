#include <cassert>
#include <cstdint>

#include "dc_frame_codec.hpp"

namespace {
    dc::FramePayload make_payload(uint8_t length, uint8_t first_value) {
        dc::FramePayload payload{};
        payload.length = length;
        for (uint8_t index = 0; index < length && index < payload.data.size(); ++index) {
            payload.data[index] = static_cast<uint8_t>(first_value + index);
        }
        return payload;
    }

    dc::Frame make_sentinel_frame() {
        dc::Frame frame{};
        frame.id = 0x12345678;
        frame.length = 8;
        for (auto& byte : frame.data) byte = 0xA5;
        return frame;
    }

    dc::FramePayload make_sentinel_payload() {
        dc::FramePayload payload{};
        payload.length = 8;
        for (auto& byte : payload.data) byte = 0x5A;
        return payload;
    }

    void assert_payload_equal(const dc::FramePayload& expected,
                              const dc::FramePayload& actual) {
        assert(expected.length == actual.length);
        assert(expected.data == actual.data);
    }

    void assert_frame_equal(const dc::Frame& expected, const dc::Frame& actual) {
        assert(expected.id == actual.id);
        assert(expected.data == actual.data);
        assert(expected.length == actual.length);
    }

    void assert_data_wire_id_equal(const dc::DataWireId& expected,
                                   const dc::DataWireId& actual) {
        assert(expected.arbitration_class == actual.arbitration_class);
        assert(expected.category == actual.category);
        assert(expected.topic == actual.topic);
        assert(expected.extension == actual.extension);
    }

    void assert_command_wire_id_equal(const dc::CommandWireId& expected,
                                      const dc::CommandWireId& actual) {
        assert(expected.arbitration_class == actual.arbitration_class);
        assert(expected.category == actual.category);
        assert(expected.topic == actual.topic);
        assert(expected.action == actual.action);
        assert(expected.extension == actual.extension);
    }

    void assert_subscription_wire_id_equal(
        const dc::SubscriptionWireId& expected,
        const dc::SubscriptionWireId& actual) {
        assert(expected.freshness == actual.freshness);
        assert(expected.category == actual.category);
        assert(expected.topic == actual.topic);
        assert(expected.requester_id == actual.requester_id);
        assert(expected.operation == actual.operation);
    }

    void test_data_zero_and_eight_byte_round_trips() {
        const dc::DataWireId wire_id{3, 0x12, 0x34, 0};
        for (const uint8_t length : {uint8_t{0}, uint8_t{2}, uint8_t{8}}) {
            const dc::FramePayload input = make_payload(length, 0x10);
            dc::Frame frame{};
            assert(dc::encode_data_frame(wire_id, input, frame) ==
                   dc::FrameCodecError::NONE);
            assert(frame.length == length);
            for (uint8_t index = length; index < frame.data.size(); ++index) {
                assert(frame.data[index] == 0);
            }

            dc::DataWireId decoded_wire_id{7, 0xAA, 0xAA, 0xAA};
            dc::FramePayload decoded_payload = make_sentinel_payload();
            assert(dc::decode_data_frame(frame, decoded_wire_id, decoded_payload) ==
                   dc::FrameCodecError::NONE);
            assert_data_wire_id_equal(wire_id, decoded_wire_id);
            assert_payload_equal(input, decoded_payload);
        }
    }

    void test_command_zero_and_eight_byte_round_trips() {
        const dc::CommandWireId wire_id{
            6, 0x45, 0x67, dc::CommandAction::TOGGLE, 0};
        for (const uint8_t length : {uint8_t{0}, uint8_t{3}, uint8_t{8}}) {
            const dc::FramePayload input = make_payload(length, 0x20);
            dc::Frame frame{};
            assert(dc::encode_command_frame(wire_id, input, frame) ==
                   dc::FrameCodecError::NONE);
            assert(frame.length == length);
            for (uint8_t index = length; index < frame.data.size(); ++index) {
                assert(frame.data[index] == 0);
            }

            dc::CommandWireId decoded_wire_id{
                7, 0xAA, 0xAA, dc::CommandAction::TRIGGER, 0xAA};
            dc::FramePayload decoded_payload = make_sentinel_payload();
            assert(dc::decode_command_frame(frame, decoded_wire_id, decoded_payload) ==
                   dc::FrameCodecError::NONE);
            assert_command_wire_id_equal(wire_id, decoded_wire_id);
            assert_payload_equal(input, decoded_payload);
        }
    }

    void test_decode_zeroes_bytes_beyond_payload_length() {
        dc::Frame frame{};
        assert(dc::encode_data_frame(
                   {0, 1, 2, 0}, make_payload(2, 0x30), frame) ==
               dc::FrameCodecError::NONE);
        for (auto& byte : frame.data) byte = 0xCC;
        frame.data[0] = 0x30;
        frame.data[1] = 0x31;

        dc::DataWireId wire_id{0, 0, 0, 0};
        dc::FramePayload payload = make_sentinel_payload();
        assert(dc::decode_data_frame(frame, wire_id, payload) ==
               dc::FrameCodecError::NONE);
        assert(payload.length == 2);
        assert(payload.data[0] == 0x30);
        assert(payload.data[1] == 0x31);
        for (uint8_t index = 2; index < payload.data.size(); ++index) {
            assert(payload.data[index] == 0);
        }
    }

    void test_subscription_zero_payload_round_trip() {
        const dc::SubscriptionWireId wire_id{
            dc::priority::MS_100, 0x89, 0xAB, 63,
            dc::SubscriptionOperation::UNSUBSCRIBE};
        dc::Frame frame = make_sentinel_frame();
        assert(dc::encode_subscription_frame(wire_id, frame) ==
               dc::FrameCodecError::NONE);
        assert(frame.length == 0);
        for (const auto byte : frame.data) assert(byte == 0);

        dc::SubscriptionWireId decoded{
            0, 0, 0, 0, dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE};
        assert(dc::decode_subscription_frame(frame, decoded) ==
               dc::FrameCodecError::NONE);
        assert(decoded.freshness == wire_id.freshness);
        assert(decoded.category == wire_id.category);
        assert(decoded.topic == wire_id.topic);
        assert(decoded.requester_id == wire_id.requester_id);
        assert(decoded.operation == wire_id.operation);
    }

    void test_encoding_failures_leave_frames_unchanged() {
        const dc::Frame sentinel = make_sentinel_frame();
        dc::Frame output = sentinel;

        assert(dc::encode_data_frame(
                   {0, 0, 0, 1}, make_payload(0, 0), output) ==
               dc::FrameCodecError::WIRE_ID_ERROR);
        assert_frame_equal(sentinel, output);

        dc::FramePayload invalid_payload = make_payload(0, 0);
        invalid_payload.length = 9;
        output = sentinel;
        assert(dc::encode_data_frame(
                   {0, 0, 0, 0}, invalid_payload, output) ==
               dc::FrameCodecError::INVALID_PAYLOAD_LENGTH);
        assert_frame_equal(sentinel, output);

        output = sentinel;
        assert(dc::encode_command_frame(
                   {0, 0, 0, dc::CommandAction::TRIGGER, 0},
                   invalid_payload, output) ==
               dc::FrameCodecError::INVALID_PAYLOAD_LENGTH);
        assert_frame_equal(sentinel, output);

        output = sentinel;
        assert(dc::encode_command_frame(
                   {0, 0, 0, static_cast<dc::CommandAction>(9), 0},
                   make_payload(0, 0), output) ==
               dc::FrameCodecError::WIRE_ID_ERROR);
        assert_frame_equal(sentinel, output);

        output = sentinel;
        assert(dc::encode_subscription_frame(
                   {5, 0, 0, 0, dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE},
                   output) == dc::FrameCodecError::WIRE_ID_ERROR);
        assert_frame_equal(sentinel, output);
    }

    void test_decoding_failures_leave_outputs_unchanged() {
        const dc::DataWireId data_sentinel{7, 0xA1, 0xA2, 0xA3};
        const dc::CommandWireId command_sentinel{
            7, 0xB1, 0xB2, dc::CommandAction::HOLD, 0xB3};
        const dc::SubscriptionWireId subscription_sentinel{
            4, 0xC1, 0xC2, 62, dc::SubscriptionOperation::UNSUBSCRIBE};
        const dc::FramePayload payload_sentinel = make_sentinel_payload();

        dc::Frame malformed = make_sentinel_frame();
        malformed.length = 9;
        dc::DataWireId data_output = data_sentinel;
        dc::FramePayload data_payload = payload_sentinel;
        assert(dc::decode_data_frame(malformed, data_output, data_payload) ==
               dc::FrameCodecError::INVALID_PAYLOAD_LENGTH);
        assert_data_wire_id_equal(data_sentinel, data_output);
        assert_payload_equal(payload_sentinel, data_payload);

        malformed.length = 0;
        assert(dc::encode_subscription_frame(
                   {0, 0, 0, 0, dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE},
                   malformed) == dc::FrameCodecError::NONE);
        data_output = data_sentinel;
        data_payload = payload_sentinel;
        assert(dc::decode_data_frame(malformed, data_output, data_payload) ==
               dc::FrameCodecError::WIRE_ID_ERROR);
        assert_data_wire_id_equal(data_sentinel, data_output);
        assert_payload_equal(payload_sentinel, data_payload);

        malformed = make_sentinel_frame();
        malformed.length = 9;
        dc::CommandWireId command_output = command_sentinel;
        dc::FramePayload command_payload = payload_sentinel;
        assert(dc::decode_command_frame(malformed, command_output, command_payload) ==
               dc::FrameCodecError::INVALID_PAYLOAD_LENGTH);
        assert_command_wire_id_equal(command_sentinel, command_output);
        assert_payload_equal(payload_sentinel, command_payload);

        malformed.length = 1;
        dc::SubscriptionWireId subscription_output = subscription_sentinel;
        assert(dc::decode_subscription_frame(malformed, subscription_output) ==
               dc::FrameCodecError::SUBSCRIPTION_PAYLOAD_NOT_EMPTY);
        assert_subscription_wire_id_equal(subscription_sentinel, subscription_output);

         malformed.length = 0;
         assert(dc::encode_data_frame(
                 {0, 0, 0, 0}, make_payload(0, 0), malformed) ==
             dc::FrameCodecError::NONE);
         command_output = command_sentinel;
         command_payload = payload_sentinel;
        assert(dc::decode_command_frame(malformed, command_output, command_payload) ==
               dc::FrameCodecError::WIRE_ID_ERROR);
        assert_command_wire_id_equal(command_sentinel, command_output);
        assert_payload_equal(payload_sentinel, command_payload);

         assert(dc::encode_command_frame(
                 {0, 0, 0, dc::CommandAction::TRIGGER, 0},
                 make_payload(0, 0), malformed) == dc::FrameCodecError::NONE);
         subscription_output = subscription_sentinel;
         assert(dc::decode_subscription_frame(malformed, subscription_output) ==
             dc::FrameCodecError::WIRE_ID_ERROR);
         assert_subscription_wire_id_equal(subscription_sentinel, subscription_output);
    }

    void test_dlc_validation_precedes_subscription_wire_validation() {
        dc::Frame frame = make_sentinel_frame();
        frame.length = 9;
        dc::SubscriptionWireId output{
            0, 0, 0, 0, dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE};
        assert(dc::decode_subscription_frame(frame, output) ==
               dc::FrameCodecError::INVALID_PAYLOAD_LENGTH);

        frame.length = 1;
        assert(dc::decode_subscription_frame(frame, output) ==
               dc::FrameCodecError::SUBSCRIPTION_PAYLOAD_NOT_EMPTY);
        assert_subscription_wire_id_equal(
            {0, 0, 0, 0, dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE}, output);

        frame.length = 8;
        assert(dc::decode_subscription_frame(frame, output) ==
               dc::FrameCodecError::SUBSCRIPTION_PAYLOAD_NOT_EMPTY);
        assert_subscription_wire_id_equal(
            {0, 0, 0, 0, dc::SubscriptionOperation::SUBSCRIBE_OR_UPDATE}, output);
    }
}

int main() {
    test_data_zero_and_eight_byte_round_trips();
    test_command_zero_and_eight_byte_round_trips();
    test_decode_zeroes_bytes_beyond_payload_length();
    test_subscription_zero_payload_round_trip();
    test_encoding_failures_leave_frames_unchanged();
    test_decoding_failures_leave_outputs_unchanged();
    test_dlc_validation_precedes_subscription_wire_validation();
}
