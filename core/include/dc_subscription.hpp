#pragma once
#include "dc_topics.hpp"
#include "dc_priority.hpp"

namespace dc {
    struct SubscriptionRequest {
        Category category;
        Topic topic;
        Priority priority;
    };
}