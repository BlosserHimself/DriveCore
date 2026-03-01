#include "dc_subscription_manager.hpp"

namespace dc {
    void SubscriptionManager::subscribe(Category c, Topic t, Priority p) {
        const uint16_t key = make_key(c, t);
        Subscription s {
            .category = c,
            .topic = t,
            .priority = p,
            .period_ms = priority_to_period_ms(p),
        };
        subscriptions_[key] = s;
    }

    void SubscriptionManager::unsubscribe(Category c, Topic t) {
        subscriptions_.erase(make_key(c, t));
    }

    bool SubscriptionManager::isSubscribed(Category c, Topic t) const {
        return subscriptions_.find(make_key(c, t)) != subscriptions_.end();
    }

    const Subscription* SubscriptionManager::get(Category c, Topic t) const {
        auto it = subscriptions_.find(make_key(c, t));
        if (it == subscriptions_.end()) return nullptr;
        return &it->second;
    }

}