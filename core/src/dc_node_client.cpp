#include "dc_node_client.hpp"

namespace dc {

    NodeClient::NodeClient(uint8_t node_id)
        : node_id_(node_id) {}

    SignalHandle<uint16_t> NodeClient::subscribe_u16(Category c, Topic t, Priority p) {
        SubscriptionRequest req { .category = c, .topic = t, .priority = p };
        subs_.push_back(req);
        return SignalHandle<uint16_t>(this, c, t);
    }

    SignalHandle<uint32_t> NodeClient::subscribe_u32(Category c, Topic t, Priority p) {
        SubscriptionRequest req { .category = c, .topic = t, .priority = p };
        subs_.push_back(req);
        return SignalHandle<uint32_t>(this, c, t);
    }

    std::vector<uint8_t> NodeClient::build_subscription_payload() const {
        // Simple wire format: sequence of (category, topic, priority)
        std::vector<uint8_t> out;
        out.reserve(subs_.size() * 3);
        for (const auto& s : subs_) {
            out.push_back(static_cast<uint8_t>(s.category));
            out.push_back(static_cast<uint8_t>(s.topic));
            out.push_back(static_cast<uint8_t>(s.priority));
        }
        return out;
    }

    void NodeClient::on_message(const Message& msg, uint32_t now_ms) {
        // Basic decode: treat payload size 2 as u16, size 4 as u32
        const auto c = msg.category;
        const auto t = msg.topic;
        if (msg.payload.size() == 2) {
            uint16_t v = static_cast<uint16_t>(msg.payload[0]) |
                         (static_cast<uint16_t>(msg.payload[1]) << 8);
            cache_.update_u16(c, t, v, now_ms);
        } else if (msg.payload.size() == 4) {
            uint32_t v = static_cast<uint32_t>(msg.payload[0]) |
                         (static_cast<uint32_t>(msg.payload[1]) << 8) |
                         (static_cast<uint32_t>(msg.payload[2]) << 16) |
                         (static_cast<uint32_t>(msg.payload[3]) << 24);
            cache_.update_u32(c, t, v, now_ms);
        } else {
            // unsupported payload size — ignore for now
        }
    }

    uint16_t NodeClient::get_u16(Category c, Topic t, uint16_t def) const {
        return cache_.get_u16(c, t, def);
    }

    uint32_t NodeClient::get_u32(Category c, Topic t, uint32_t def) const {
        return cache_.get_u32(c, t, def);
    }

    bool NodeClient::has(Category c, Topic t) const {
        return cache_.has_value(c, t);
    }

}