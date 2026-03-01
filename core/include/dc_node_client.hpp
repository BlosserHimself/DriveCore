#pragma once
#include <vector>
#include <cstdint>

#include "dc_subscription.hpp"
#include "dc_signal_cache.hpp"
#include "dc_message.hpp"

namespace dc {
    template<typename T>
    class SignalHandle {
        public:
            SignalHandle(NodeClient* client, Category c, Topic t)
                : client_(client), c_(c), t_(t) {}

            T get (T def = 0) const;

        private:
            NodeClient* client_;
            Category c_;
            Topic t_;
    };

    class NodeClient {
        public:
            explicit NodeClient(uint8_t node_id = 0);

            // Subscription declarations
            SignalHandle<uint16_t> subscribe_u16(Category c, Topic t, Priority p);
            SignalHandle<uint32_t> subscribe_u32(Category c, Topic t, Priority p);

            // Registration payload builder for startup handshake
            std::vector<uint8_t> build_subscription_payload() const;

            // Message input from CAN (after decoding)
            void on_message(const Message& msg, uint32_t now_ms);

            // typed access (used by handles)
            uint16_t get_u16(Category c, Topic t, uint16_t def = 0) const;
            uint32_t get_u32(Category c, Topic t, uint32_t def = 0) const;

            bool has(Category c, Topic t) const;

        private:
            uint8_t node_id_;
            std::vector<SubscriptionRequest> subs_;
            SignalCache32 cache_;
    };

    template<>
    inline uint16_t SignalHandle<uint16_t>::get(uint16_t def) const {
        return client_->get_u16(c_, t_, def);
    }

    template<>
    inline uint32_t SignalHandle<uint32_t>::get(uint32_t def) const {
        return client_->get_u32(c_, t_, def);
    }
}