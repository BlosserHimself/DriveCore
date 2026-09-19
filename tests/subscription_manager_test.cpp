#include <cassert>

#include "dc_subscription_manager.hpp"

namespace {
    constexpr dc::Category sensor = dc::category::SENSOR;
    constexpr dc::Topic rpm = dc::topic::sensor::RPM;

    void test_multiple_requesters_and_effective_priority() {
        dc::SubscriptionManager manager;

        manager.subscribe(1, sensor, rpm, dc::priority::MS_100);
        manager.subscribe(2, sensor, rpm, dc::priority::MS_10);

        const dc::Subscription* subscription = manager.get(sensor, rpm);
        assert(subscription != nullptr);
        assert(subscription->priority == dc::priority::MS_10);
        assert(subscription->period_ms == 10);
    }

    void test_priority_promotion_and_demotion() {
        dc::SubscriptionManager manager;

        manager.subscribe(1, sensor, rpm, dc::priority::MS_250);
        manager.subscribe(2, sensor, rpm, dc::priority::MS_10);
        manager.subscribe(2, sensor, rpm, dc::priority::MS_1000);

        const dc::Subscription* subscription = manager.get(sensor, rpm);
        assert(subscription != nullptr);
        assert(subscription->priority == dc::priority::MS_250);

        manager.subscribe(3, sensor, rpm, dc::priority::REALTIME);
        subscription = manager.get(sensor, rpm);
        assert(subscription->priority == dc::priority::REALTIME);

        manager.unsubscribe(3, sensor, rpm);
        subscription = manager.get(sensor, rpm);
        assert(subscription->priority == dc::priority::MS_250);
    }

    void test_requester_removal_and_complete_topic_removal() {
        dc::SubscriptionManager manager;

        manager.subscribe(1, sensor, rpm, dc::priority::MS_100);
        manager.subscribe(2, sensor, rpm, dc::priority::MS_10);
        manager.unsubscribe_all(2);

        const dc::Subscription* subscription = manager.get(sensor, rpm);
        assert(subscription != nullptr);
        assert(subscription->priority == dc::priority::MS_100);

        manager.unsubscribe_all(1);
        assert(!manager.isSubscribed(sensor, rpm));
        assert(manager.get(sensor, rpm) == nullptr);
    }

    void test_legacy_topic_api_remains_available() {
        dc::SubscriptionManager manager;

        manager.subscribe(sensor, rpm, dc::priority::MS_100);
        manager.subscribe(1, sensor, rpm, dc::priority::MS_10);
        manager.unsubscribe(sensor, rpm);

        const dc::Subscription* subscription = manager.get(sensor, rpm);
        assert(subscription != nullptr);
        assert(subscription->priority == dc::priority::MS_10);

        manager.unsubscribe(1, sensor, rpm);
        assert(!manager.isSubscribed(sensor, rpm));
    }
}

int main() {
    test_multiple_requesters_and_effective_priority();
    test_priority_promotion_and_demotion();
    test_requester_removal_and_complete_topic_removal();
    test_legacy_topic_api_remains_available();
}
