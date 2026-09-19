#pragma once
#include <cstdint>

#include "dc_priority.hpp"
#include "dc_topics.hpp"

namespace dc {
    enum class WireKind : uint8_t {
        DATA = 0,
        SUBSCRIPTION = 1,
        COMMAND = 2,
        SYSTEM = 3,
    };

    enum class SubscriptionOperation : uint8_t {
        SUBSCRIBE_OR_UPDATE = 0,
        UNSUBSCRIBE = 1,
    };

    enum class CommandAction : uint8_t {
        TRIGGER = 0,
        PRESS = 1,
        HOLD = 2,
        BUTTON_DOWN = 3,
        BUTTON_UP = 4,
        SET = 5,
        INCREMENT = 6,
        DECREMENT = 7,
        TOGGLE = 8,
    };

    enum class WireIdError : uint8_t {
        NONE,
        VALUE_OUT_OF_RANGE,
        WRONG_KIND,
        RESERVED_FRESHNESS,
        RESERVED_OPERATION,
        NONZERO_DATA_EXTENSION,
        RESERVED_COMMAND_ACTION,
        NONZERO_COMMAND_EXTENSION,
    };

    struct DataWireId {
        uint8_t arbitration_class;
        Category category;
        Topic topic;
        uint8_t extension = 0;
    };

    struct SubscriptionWireId {
        Priority freshness;
        Category category;
        Topic topic;
        uint8_t requester_id;
        SubscriptionOperation operation;
    };

    struct CommandWireId {
        uint8_t arbitration_class;
        Category category;
        Topic topic;
        CommandAction action;
        uint8_t extension = 0;
    };

    namespace wire_id {
        constexpr uint32_t MAX_EXTENDED_ID = 0x1FFFFFFFu;

        constexpr uint32_t POLICY_SHIFT = 26;
        constexpr uint32_t KIND_SHIFT = 24;
        constexpr uint32_t CATEGORY_SHIFT = 16;
        constexpr uint32_t TOPIC_SHIFT = 8;
        constexpr uint32_t REQUESTER_SHIFT = 2;
        constexpr uint32_t COMMAND_ACTION_SHIFT = 4;

        constexpr uint32_t POLICY_MASK = 0x07u;
        constexpr uint32_t KIND_MASK = 0x03u;
        constexpr uint32_t BYTE_MASK = 0xFFu;
        constexpr uint32_t REQUESTER_MASK = 0x3Fu;
        constexpr uint32_t OPERATION_MASK = 0x03u;
        constexpr uint32_t COMMAND_ACTION_MASK = 0x0Fu;
        constexpr uint32_t COMMAND_EXTENSION_MASK = 0x0Fu;

        constexpr uint32_t field(uint32_t value, uint32_t mask, uint32_t shift) {
            return (value & mask) << shift;
        }

        constexpr bool is_valid_priority(Priority freshness) {
            return static_cast<uint32_t>(freshness) <= 4u;
        }

        constexpr bool is_valid_operation(SubscriptionOperation operation) {
            return static_cast<uint32_t>(operation) <= 1u;
        }

        constexpr bool is_valid_command_action(CommandAction action) {
            return static_cast<uint32_t>(action) <= 8u;
        }
    }

    inline WireIdError encode_data_id(const DataWireId& fields, uint32_t& id) {
        const uint32_t arbitration_class = fields.arbitration_class;
        const uint32_t category = fields.category;
        const uint32_t topic = fields.topic;
        const uint32_t extension = fields.extension;

        if (arbitration_class > wire_id::POLICY_MASK ||
            category > wire_id::BYTE_MASK ||
            topic > wire_id::BYTE_MASK) {
            return WireIdError::VALUE_OUT_OF_RANGE;
        }
        if (extension != 0u) {
            return WireIdError::NONZERO_DATA_EXTENSION;
        }

        id = wire_id::field(arbitration_class, wire_id::POLICY_MASK,
                            wire_id::POLICY_SHIFT) |
             wire_id::field(static_cast<uint32_t>(WireKind::DATA),
                            wire_id::KIND_MASK, wire_id::KIND_SHIFT) |
             wire_id::field(category, wire_id::BYTE_MASK,
                            wire_id::CATEGORY_SHIFT) |
             wire_id::field(topic, wire_id::BYTE_MASK,
                            wire_id::TOPIC_SHIFT);
        return WireIdError::NONE;
    }

    inline WireIdError decode_data_id(uint32_t id, DataWireId& fields) {
        if (id > wire_id::MAX_EXTENDED_ID) {
            return WireIdError::VALUE_OUT_OF_RANGE;
        }

        const uint32_t kind = (id >> wire_id::KIND_SHIFT) & wire_id::KIND_MASK;
        if (kind != static_cast<uint32_t>(WireKind::DATA)) {
            return WireIdError::WRONG_KIND;
        }

        const uint32_t extension = id & wire_id::BYTE_MASK;
        if (extension != 0u) {
            return WireIdError::NONZERO_DATA_EXTENSION;
        }

        DataWireId decoded{
            static_cast<uint8_t>((id >> wire_id::POLICY_SHIFT) & wire_id::POLICY_MASK),
            static_cast<Category>((id >> wire_id::CATEGORY_SHIFT) & wire_id::BYTE_MASK),
            static_cast<Topic>((id >> wire_id::TOPIC_SHIFT) & wire_id::BYTE_MASK),
            static_cast<uint8_t>(extension),
        };
        fields = decoded;
        return WireIdError::NONE;
    }

    inline WireIdError encode_subscription_id(
        const SubscriptionWireId& fields, uint32_t& id) {
        const uint32_t freshness = fields.freshness;
        const uint32_t category = fields.category;
        const uint32_t topic = fields.topic;
        const uint32_t requester_id = fields.requester_id;
        const uint32_t operation = static_cast<uint32_t>(fields.operation);

        if (category > wire_id::BYTE_MASK || topic > wire_id::BYTE_MASK ||
            requester_id > wire_id::REQUESTER_MASK) {
            return WireIdError::VALUE_OUT_OF_RANGE;
        }
        if (!wire_id::is_valid_priority(fields.freshness)) {
            return WireIdError::RESERVED_FRESHNESS;
        }
        if (!wire_id::is_valid_operation(fields.operation)) {
            return WireIdError::RESERVED_OPERATION;
        }

        id = wire_id::field(freshness, wire_id::POLICY_MASK,
                            wire_id::POLICY_SHIFT) |
             wire_id::field(static_cast<uint32_t>(WireKind::SUBSCRIPTION),
                            wire_id::KIND_MASK, wire_id::KIND_SHIFT) |
             wire_id::field(category, wire_id::BYTE_MASK,
                            wire_id::CATEGORY_SHIFT) |
             wire_id::field(topic, wire_id::BYTE_MASK,
                            wire_id::TOPIC_SHIFT) |
             wire_id::field(requester_id, wire_id::REQUESTER_MASK,
                            wire_id::REQUESTER_SHIFT) |
             operation;
        return WireIdError::NONE;
    }

    inline WireIdError decode_subscription_id(
        uint32_t id, SubscriptionWireId& fields) {
        if (id > wire_id::MAX_EXTENDED_ID) {
            return WireIdError::VALUE_OUT_OF_RANGE;
        }

        const uint32_t kind = (id >> wire_id::KIND_SHIFT) & wire_id::KIND_MASK;
        if (kind != static_cast<uint32_t>(WireKind::SUBSCRIPTION)) {
            return WireIdError::WRONG_KIND;
        }

        const uint32_t freshness = (id >> wire_id::POLICY_SHIFT) & wire_id::POLICY_MASK;
        if (freshness > 4u) {
            return WireIdError::RESERVED_FRESHNESS;
        }

        const uint32_t operation = id & wire_id::OPERATION_MASK;
        if (operation > 1u) {
            return WireIdError::RESERVED_OPERATION;
        }

        SubscriptionWireId decoded{
            static_cast<Priority>(freshness),
            static_cast<Category>((id >> wire_id::CATEGORY_SHIFT) & wire_id::BYTE_MASK),
            static_cast<Topic>((id >> wire_id::TOPIC_SHIFT) & wire_id::BYTE_MASK),
            static_cast<uint8_t>((id >> wire_id::REQUESTER_SHIFT) & wire_id::REQUESTER_MASK),
            static_cast<SubscriptionOperation>(operation),
        };
        fields = decoded;
        return WireIdError::NONE;
    }

    inline WireIdError encode_command_id(const CommandWireId& fields, uint32_t& id) {
        const uint32_t arbitration_class = fields.arbitration_class;
        const uint32_t category = fields.category;
        const uint32_t topic = fields.topic;
        const uint32_t action = static_cast<uint32_t>(fields.action);
        const uint32_t extension = fields.extension;

        if (arbitration_class > wire_id::POLICY_MASK ||
            category > wire_id::BYTE_MASK || topic > wire_id::BYTE_MASK) {
            return WireIdError::VALUE_OUT_OF_RANGE;
        }
        if (!wire_id::is_valid_command_action(fields.action)) {
            return WireIdError::RESERVED_COMMAND_ACTION;
        }
        if (extension != 0u) {
            return WireIdError::NONZERO_COMMAND_EXTENSION;
        }

        id = wire_id::field(arbitration_class, wire_id::POLICY_MASK,
                            wire_id::POLICY_SHIFT) |
             wire_id::field(static_cast<uint32_t>(WireKind::COMMAND),
                            wire_id::KIND_MASK, wire_id::KIND_SHIFT) |
             wire_id::field(category, wire_id::BYTE_MASK,
                            wire_id::CATEGORY_SHIFT) |
             wire_id::field(topic, wire_id::BYTE_MASK,
                            wire_id::TOPIC_SHIFT) |
             wire_id::field(action, wire_id::COMMAND_ACTION_MASK,
                            wire_id::COMMAND_ACTION_SHIFT);
        return WireIdError::NONE;
    }

    inline WireIdError decode_command_id(uint32_t id, CommandWireId& fields) {
        if (id > wire_id::MAX_EXTENDED_ID) {
            return WireIdError::VALUE_OUT_OF_RANGE;
        }

        const uint32_t kind = (id >> wire_id::KIND_SHIFT) & wire_id::KIND_MASK;
        if (kind != static_cast<uint32_t>(WireKind::COMMAND)) {
            return WireIdError::WRONG_KIND;
        }

        const uint32_t action = (id >> wire_id::COMMAND_ACTION_SHIFT) &
                                wire_id::COMMAND_ACTION_MASK;
        if (action > 8u) {
            return WireIdError::RESERVED_COMMAND_ACTION;
        }

        const uint32_t extension = id & wire_id::COMMAND_EXTENSION_MASK;
        if (extension != 0u) {
            return WireIdError::NONZERO_COMMAND_EXTENSION;
        }

        CommandWireId decoded{
            static_cast<uint8_t>((id >> wire_id::POLICY_SHIFT) & wire_id::POLICY_MASK),
            static_cast<Category>((id >> wire_id::CATEGORY_SHIFT) & wire_id::BYTE_MASK),
            static_cast<Topic>((id >> wire_id::TOPIC_SHIFT) & wire_id::BYTE_MASK),
            static_cast<CommandAction>(action),
            static_cast<uint8_t>(extension),
        };
        fields = decoded;
        return WireIdError::NONE;
    }
}
