#pragma once
#include <cstdint>
#include <unordered_map>

#include "dc_topics.hpp"
#include "dc_priority.hpp"

namespace dc {

    struct Subscription {
        Category category;
        Topic topic;
        Priority priority;
        uint32_t period_ms;
    };

    class SubscriptionManager {
        public:
            using RequesterId = uint8_t;

            void subscribe (Category c, Topic t, Priority p = priority::MS_100);
            void subscribe (RequesterId requester_id, Category c, Topic t,
                            Priority p);

            void unsubscribe(Category c, Topic t);
            void unsubscribe(RequesterId requester_id, Category c, Topic t);
            void unsubscribe_all(RequesterId requester_id);

            bool isSubscribed(Category c, Topic t) const;

            // Get subscription metadata (nullptr if not subscribed)
            const Subscription* get(Category c, Topic t) const;

            // Usefule for carrier scheduling loops
            template <typename Fn>
            void forEach(Fn&& fn) const {
                for (const auto& kv : subscriptions_) fn(kv.second);
            }

        private:
            using RequesterKey = uint16_t;

            static constexpr uint16_t make_key(Category c, Topic t) {
                return (static_cast<uint16_t>(c) << 8) | static_cast<uint16_t>(t);
            }

            static constexpr RequesterKey legacy_requester = 0x100;

            void set_request(RequesterKey requester_id, Category c, Topic t,
                             Priority p);
            void remove_request(RequesterKey requester_id, Category c, Topic t);
            void recalculate(uint16_t key);

            std::unordered_map<uint16_t, Subscription> subscriptions_;
            std::unordered_map<uint16_t, std::unordered_map<RequesterKey, Priority>> requests_;
    };
}