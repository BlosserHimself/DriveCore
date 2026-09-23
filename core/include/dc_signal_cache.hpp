#pragma once
#include <atomic>
#include <concepts>
#include <cstdint>
#include <unordered_map>

#include "dc_signal_key.hpp"
#include "dc_topics.hpp"

namespace dc {
    struct SignalEntry32 {
        std::atomic<uint32_t> value{0};
        std::atomic<uint32_t> last_rx_ms{0};
        std::atomic<uint8_t> valid{0};
    };

    class SignalCache32 {
        public:
            // Called by Rx handler when msg arrives
            void update_u16(Category c, Topic t, uint16_t v, uint32_t now_ms);
            void update_u32(Category c, Topic t, uint32_t v, uint32_t now_ms);

            // Called by UI / app
            uint16_t get_u16(Category c, Topic t, uint16_t default_v = 0) const;
            uint32_t get_u32(Category c, Topic t, uint32_t default_v = 0) const;
            
            bool has_value(Category c, Topic t) const;
            uint32_t age_ms(Category c, Topic t, uint32_t now_ms) const;

            // Transitional: satisfies SignalReadable<SignalCache32, T> for
            // exactly its two existing supported types, so SignalHandle can
            // read this cache without SignalCache32 becoming the canonical
            // future SignalStore implementation.
            template <typename ValueType>
            ValueType read(const SignalKey& key, ValueType fallback) const
                requires (std::same_as<ValueType, uint16_t> ||
                          std::same_as<ValueType, uint32_t>) {
                if constexpr (std::same_as<ValueType, uint16_t>) {
                    return get_u16(key.category, key.topic, fallback);
                } else {
                    return get_u32(key.category, key.topic, fallback);
                }
            }

        private:
            SignalEntry32& entry(Category c, Topic t);
            const SignalEntry32* entry_if_exists(Category c, Topic t) const;

            std::unordered_map<uint16_t, SignalEntry32> map_;
    };

}