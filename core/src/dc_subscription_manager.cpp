#include "dc_subscription_manager.hpp"

namespace dc {
    void SubscriptionManager::subscribe(Category c, Topic t, Priority p) {
        set_request(legacy_requester, c, t, p);
    }

    void SubscriptionManager::subscribe(RequesterId requester_id,
                                         Category c, Topic t, Priority p) {
        set_request(requester_id, c, t, p);
    }

    void SubscriptionManager::unsubscribe(Category c, Topic t) {
        remove_request(legacy_requester, c, t);
    }

    void SubscriptionManager::unsubscribe(RequesterId requester_id,
                                           Category c, Topic t) {
        remove_request(requester_id, c, t);
    }

    void SubscriptionManager::unsubscribe_all(RequesterId requester_id) {
        for (auto requests_it = requests_.begin(); requests_it != requests_.end();) {
            requests_it->second.erase(requester_id);
            if (requests_it->second.empty()) {
                subscriptions_.erase(requests_it->first);
                requests_it = requests_.erase(requests_it);
            } else {
                recalculate(requests_it->first);
                ++requests_it;
            }
        }
    }

    bool SubscriptionManager::isSubscribed(Category c, Topic t) const {
        return subscriptions_.find(make_key(c, t)) != subscriptions_.end();
    }

    const Subscription* SubscriptionManager::get(Category c, Topic t) const {
        auto it = subscriptions_.find(make_key(c, t));
        if (it == subscriptions_.end()) return nullptr;
        return &it->second;
    }

    void SubscriptionManager::set_request(RequesterKey requester_id,
                                           Category c, Topic t, Priority p) {
        const uint16_t key = make_key(c, t);
        requests_[key][requester_id] = p;
        recalculate(key);
    }

    void SubscriptionManager::remove_request(RequesterKey requester_id,
                                              Category c, Topic t) {
        const uint16_t key = make_key(c, t);
        auto requests_it = requests_.find(key);
        if (requests_it == requests_.end()) return;

        requests_it->second.erase(requester_id);
        if (requests_it->second.empty()) {
            requests_.erase(requests_it);
            subscriptions_.erase(key);
            return;
        }

        recalculate(key);
    }

    void SubscriptionManager::recalculate(uint16_t key) {
        const auto requests_it = requests_.find(key);
        if (requests_it == requests_.end() || requests_it->second.empty()) {
            subscriptions_.erase(key);
            return;
        }

        Priority effective_priority = requests_it->second.begin()->second;
        for (const auto& request : requests_it->second) {
            if (request.second < effective_priority) {
                effective_priority = request.second;
            }
        }

        const Category category = static_cast<Category>(key >> 8);
        const Topic topic = static_cast<Topic>(key & 0xFF);
        subscriptions_[key] = Subscription{
            category,
            topic,
            effective_priority,
            priority_to_period_ms(effective_priority),
        };
    }

}