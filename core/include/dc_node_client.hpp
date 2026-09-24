#pragma once
#include <vector>
#include <cstdint>

#include "dc_dispatcher.hpp"
#include "dc_frame_codec.hpp"
#include "dc_subscription.hpp"
#include "dc_signal_cache.hpp"
#include "dc_signal_handle.hpp"
#include "dc_message.hpp"

namespace dc {
    class NodeClient {
        public:
            enum class SubscriptionSendError : uint8_t {
                NONE,
                NO_DISPATCHER,
                INVALID_REQUESTER_ID,
                FRAME_CODEC_ERROR,
                PENDING_FULL,
            };

            explicit NodeClient(uint16_t requester_id = 0);
            NodeClient(Dispatcher& dispatcher, uint16_t requester_id = 0);

            // Subscription declarations. Returned handles reference this
            // NodeClient's internally-owned cache directly (transitional:
            // NodeClient's cache ownership is not addressed by this change).
            SignalHandle<SignalCache32, uint16_t> subscribe_u16(Category c, Topic t, Priority p);
            SignalHandle<SignalCache32, uint32_t> subscribe_u32(Category c, Topic t, Priority p);

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
            Dispatcher* dispatcher_;
            std::vector<SubscriptionRequest> subs_;
            SignalCache32 cache_;
    };
}