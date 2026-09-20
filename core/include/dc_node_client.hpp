#pragma once
#include <vector>
#include <cstdint>

#include "dc_bus.hpp"
#include "dc_frame_codec.hpp"
#include "dc_subscription.hpp"
#include "dc_signal_cache.hpp"
#include "dc_message.hpp"

namespace dc {
    class NodeClient;

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
            enum class SubscriptionSendError : uint8_t {
                NONE,
                NO_BUS,
                INVALID_REQUESTER_ID,
                FRAME_CODEC_ERROR,
                BUS_SEND_FAILED,
            };

            explicit NodeClient(uint16_t requester_id = 0);
            NodeClient(IBus& bus, uint16_t requester_id = 0);

            // Subscription declarations
            SignalHandle<uint16_t> subscribe_u16(Category c, Topic t, Priority p);
            SignalHandle<uint32_t> subscribe_u32(Category c, Topic t, Priority p);

            SubscriptionSendError send_subscription(
                Category c, Topic t, Priority p);
            SubscriptionSendError send_unsubscribe(Category c, Topic t);

            // Message input from CAN (after decoding)
            void on_message(const Message& msg, uint32_t now_ms);

            // typed access (used by handles)
            uint16_t get_u16(Category c, Topic t, uint16_t def = 0) const;
            uint32_t get_u32(Category c, Topic t, uint32_t def = 0) const;

            bool has(Category c, Topic t) const;

        private:
            SubscriptionSendError send_subscription_frame(
                Category c, Topic t, Priority p, SubscriptionOperation operation);
            void remove_local_subscriptions(Category c, Topic t);

            uint16_t requester_id_;
            IBus* bus_;
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