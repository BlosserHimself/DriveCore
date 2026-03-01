#include "dc_subscription_manager.hpp"

namespace dc {
    void SubscriptionManager::subscribe(Category c, Topic t) {
        uint16_t key = (static_cast<uint16_t>(c) << 8) | t;
        subscriptions.insert(key);
    }

    void SubscriptionManager::unsubscribe(Category c, Topic t) {
        uint16_t key = (static_cast<uint16_t>(c) << 8) | t;
        subscriptions.erase(key);
    }

    bool SubscriptionManager::isSubscribed(Category c, Topic t) const {
        uint16_t key = (static_cast<uint16_t>(c) << 8) | t;
        return subscriptions.find(key) != subscriptions.end();
    }
}