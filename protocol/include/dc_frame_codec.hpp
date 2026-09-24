#pragma once
#include <array>
#include <cstdint>

#include "dc_frame.hpp"
#include "dc_wire_id.hpp"

namespace dc {
    struct FramePayload {
        std::array<uint8_t, 8> data{};
        uint8_t length = 0;
    };

    enum class FrameCodecError : uint8_t {
        NONE,
        INVALID_PAYLOAD_LENGTH,
        SUBSCRIPTION_PAYLOAD_NOT_EMPTY,
        WIRE_ID_ERROR,
    };

    namespace frame_codec_detail {
        inline FrameCodecError map_wire_id_error(WireIdError error) {
            switch (error) {
                case WireIdError::NONE:
                    return FrameCodecError::NONE;
                case WireIdError::VALUE_OUT_OF_RANGE:
                case WireIdError::WRONG_KIND:
                case WireIdError::RESERVED_FRESHNESS:
                case WireIdError::RESERVED_OPERATION:
                case WireIdError::NONZERO_DATA_EXTENSION:
                case WireIdError::RESERVED_COMMAND_ACTION:
                case WireIdError::NONZERO_COMMAND_EXTENSION:
                    return FrameCodecError::WIRE_ID_ERROR;
            }

            return FrameCodecError::WIRE_ID_ERROR;
        }

        inline bool valid_payload_length(const FramePayload& payload) {
            return payload.length <= payload.data.size();
        }

        inline bool valid_frame_length(const Frame& frame) {
            return frame.length <= frame.data.size();
        }
    }

    inline FrameCodecError encode_data_frame(
        const DataWireId& wire_id,
        const FramePayload& payload,
        Frame& out_frame) {
        if (!frame_codec_detail::valid_payload_length(payload)) {
            return FrameCodecError::INVALID_PAYLOAD_LENGTH;
        }

        uint32_t id = 0;
        const WireIdError wire_error = encode_data_id(wire_id, id);
        if (wire_error != WireIdError::NONE) {
            return frame_codec_detail::map_wire_id_error(wire_error);
        }

        Frame encoded{};
        encoded.id = id;
        encoded.length = payload.length;
        for (uint8_t index = 0; index < payload.length; ++index) {
            encoded.data[index] = payload.data[index];
        }
        out_frame = encoded;
        return FrameCodecError::NONE;
    }

    inline FrameCodecError decode_data_frame(
        const Frame& frame,
        DataWireId& out_wire_id,
        FramePayload& out_payload) {
        if (!frame_codec_detail::valid_frame_length(frame)) {
            return FrameCodecError::INVALID_PAYLOAD_LENGTH;
        }

        DataWireId decoded_wire_id{0, 0, 0, 0};
        const WireIdError wire_error = decode_data_id(frame.id, decoded_wire_id);
        if (wire_error != WireIdError::NONE) {
            return frame_codec_detail::map_wire_id_error(wire_error);
        }

        FramePayload decoded_payload{};
        decoded_payload.length = frame.length;
        for (uint8_t index = 0; index < frame.length; ++index) {
            decoded_payload.data[index] = frame.data[index];
        }
        out_wire_id = decoded_wire_id;
        out_payload = decoded_payload;
        return FrameCodecError::NONE;
    }

    inline FrameCodecError encode_subscription_frame(
        const SubscriptionWireId& wire_id,
        Frame& out_frame) {
        uint32_t id = 0;
        const WireIdError wire_error = encode_subscription_id(wire_id, id);
        if (wire_error != WireIdError::NONE) {
            return frame_codec_detail::map_wire_id_error(wire_error);
        }

        Frame encoded{};
        encoded.id = id;
        encoded.length = 0;
        out_frame = encoded;
        return FrameCodecError::NONE;
    }

    inline FrameCodecError decode_subscription_frame(
        const Frame& frame,
        SubscriptionWireId& out_wire_id) {
        if (!frame_codec_detail::valid_frame_length(frame)) {
            return FrameCodecError::INVALID_PAYLOAD_LENGTH;
        }
        if (frame.length != 0) {
            return FrameCodecError::SUBSCRIPTION_PAYLOAD_NOT_EMPTY;
        }

        SubscriptionWireId decoded_wire_id{
            0, 0, 0, 0, SubscriptionOperation::SUBSCRIBE_OR_UPDATE};
        const WireIdError wire_error =
            decode_subscription_id(frame.id, decoded_wire_id);
        if (wire_error != WireIdError::NONE) {
            return frame_codec_detail::map_wire_id_error(wire_error);
        }

        out_wire_id = decoded_wire_id;
        return FrameCodecError::NONE;
    }

    inline FrameCodecError encode_command_frame(
        const CommandWireId& wire_id,
        const FramePayload& payload,
        Frame& out_frame) {
        if (!frame_codec_detail::valid_payload_length(payload)) {
            return FrameCodecError::INVALID_PAYLOAD_LENGTH;
        }

        uint32_t id = 0;
        const WireIdError wire_error = encode_command_id(wire_id, id);
        if (wire_error != WireIdError::NONE) {
            return frame_codec_detail::map_wire_id_error(wire_error);
        }

        Frame encoded{};
        encoded.id = id;
        encoded.length = payload.length;
        for (uint8_t index = 0; index < payload.length; ++index) {
            encoded.data[index] = payload.data[index];
        }
        out_frame = encoded;
        return FrameCodecError::NONE;
    }

    inline FrameCodecError decode_command_frame(
        const Frame& frame,
        CommandWireId& out_wire_id,
        FramePayload& out_payload) {
        if (!frame_codec_detail::valid_frame_length(frame)) {
            return FrameCodecError::INVALID_PAYLOAD_LENGTH;
        }

        CommandWireId decoded_wire_id{0, 0, 0, CommandAction::TRIGGER, 0};
        const WireIdError wire_error = decode_command_id(frame.id, decoded_wire_id);
        if (wire_error != WireIdError::NONE) {
            return frame_codec_detail::map_wire_id_error(wire_error);
        }

        FramePayload decoded_payload{};
        decoded_payload.length = frame.length;
        for (uint8_t index = 0; index < frame.length; ++index) {
            decoded_payload.data[index] = frame.data[index];
        }
        out_wire_id = decoded_wire_id;
        out_payload = decoded_payload;
        return FrameCodecError::NONE;
    }
}
