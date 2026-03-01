#pragma once
#include <cstdint>
#include <set>
#include "dc_topics.hpp"

namespace dc {
    class SubscriptionManager {
        public:
            void subscribe (Category c, Topic t);
            void unsubscribe (Category c, Topic t);
            bool isSubscribed(Category c, Topic t) const;
        
        private:
            std::set<uint16_t> subscriptions;
    };
}