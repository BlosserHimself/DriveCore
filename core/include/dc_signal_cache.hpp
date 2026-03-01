#pragma once
#include <cstdint>
#include <unordered_map>
#include <atomic>

#include "dc_topics.hpp"

namespace dc {
    struct SignalKey {
        uint16_t key; // (category<<8) | topic
        static constexpr uint16_t make(Category c, Topic t) {
            return (static_cast<uint16_t>(c) << 8) | static_cast<uint16_t>(t);
        }
    };

    struct SignalEntry32 {
        std::atomic<uint32_t> value{0};
        std::atomic<uint32_t> last_rx_ms{0};
        std::atomic<uint8_t> valid{0};
    };

    class SignalCache32 {
        public:
            // Called by Rx handler when msg arrives
            void update_u16(Category c, Topic t, uint16_t v, uint32_t now_ms);
            void update_u32(Category c, Topic t, uint16_t v, uint32_t now_ms);

            // Called by UI / app
            uint16_t get_u16(Category c, Topic t, uint16_t default_v = 0) const;
            uint32_t get_u32(Category c, Topic t, uint32_t default_v = 0) const;
            
            bool has_value(Category c, Topic t) const;
            uint32_t age_ms(Category c, Topic t, uint32_t now_ms) const;

        private:
            SignalEntry32& entry(Category c, Topic t);
            const SignalEntry32* entry_if_exists(Category c, Topic t) const;

            std::unordered_map<uint16_t, SignalEntry32> map_;
    };

}