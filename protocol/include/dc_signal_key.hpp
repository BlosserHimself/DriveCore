#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

#include "dc_topics.hpp"

namespace dc {
    // Identity of one logical DriveCore signal. Carries no value, logical
    // type, wire representation, priority, requester, or validity state.
    struct SignalKey {
        Category category;
        Topic topic;

        constexpr uint16_t packed() const {
            return (static_cast<uint16_t>(category) << 8) |
                   static_cast<uint16_t>(topic);
        }

        static constexpr SignalKey unpack(uint16_t packed) {
            return SignalKey{
                static_cast<Category>(packed >> 8),
                static_cast<Topic>(packed & 0xFF),
            };
        }

        friend constexpr bool operator==(const SignalKey& a, const SignalKey& b) {
            return a.category == b.category && a.topic == b.topic;
        }

        friend constexpr bool operator!=(const SignalKey& a, const SignalKey& b) {
            return !(a == b);
        }
    };

    struct SignalKeyHash {
        size_t operator()(const SignalKey& key) const noexcept {
            return std::hash<uint16_t>{}(key.packed());
        }
    };
}
