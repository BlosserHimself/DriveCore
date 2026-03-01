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
            void subscribe (Category c, Topic t, Priority p = priority::MS_100);

            void unsubscribe(Category c, Topic t);

            bool isSubscribed(Category c, Topic t) const;

            // Get subscription metadata (nullptr if not subscribed)
            const Subscription* get(Category c, Topic t) const;

            // Usefule for carrier scheduling loops
            template <typename Fn>
            void forEach(Fn&& fn) const {
                for (const auto& kv : subscriptions_) fn(kv.second);
            }

        private:
            static constexpr uint16_t make_key(Category c, Topic t) {
                return (static_cast<uint16_t>(c) << 8) | static_cast<uint16_t>(t);
            }

            std::unordered_map<uint16_t, Subscription> subscriptions_;
    };
}