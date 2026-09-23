#include "dc_node_client.hpp"

namespace dc {

    NodeClient::NodeClient(uint16_t requester_id)
        : requester_id_(requester_id), bus_(nullptr) {}

    NodeClient::NodeClient(IBus& bus, uint16_t requester_id)
        : requester_id_(requester_id), bus_(&bus) {}

    SignalHandle<SignalCache32, uint16_t> NodeClient::subscribe_u16(Category c, Topic t, Priority p) {
        SubscriptionRequest req { c, t, p };
        subs_.push_back(req);
        return SignalHandle<SignalCache32, uint16_t>(cache_, SignalKey{c, t});
    }

    SignalHandle<SignalCache32, uint32_t> NodeClient::subscribe_u32(Category c, Topic t, Priority p) {
        SubscriptionRequest req { c, t, p };
        subs_.push_back(req);
        return SignalHandle<SignalCache32, uint32_t>(cache_, SignalKey{c, t});
    }

    NodeClient::SubscriptionSendError NodeClient::send_subscription(
        Category c, Topic t, Priority p) {
        return send_subscription_frame(
            c, t, p, SubscriptionOperation::SUBSCRIBE_OR_UPDATE);
    }

    NodeClient::SubscriptionSendError NodeClient::send_unsubscribe(
        Category c, Topic t) {
        const SubscriptionSendError result = send_subscription_frame(
            c, t, priority::REALTIME, SubscriptionOperation::UNSUBSCRIBE);
        if (result == SubscriptionSendError::NONE) {
            remove_local_subscriptions(c, t);
        }
        return result;
    }

    NodeClient::SubscriptionSendError NodeClient::send_subscription_frame(
        Category c, Topic t, Priority p, SubscriptionOperation operation) {
        if (!bus_) return SubscriptionSendError::NO_BUS;
        if (requester_id_ > 63) {
            return SubscriptionSendError::INVALID_REQUESTER_ID;
        }

        const SubscriptionWireId wire_id{
            p, c, t, static_cast<uint8_t>(requester_id_), operation};
        Frame frame{};
        if (encode_subscription_frame(wire_id, frame) != FrameCodecError::NONE) {
            return SubscriptionSendError::FRAME_CODEC_ERROR;
        }
        if (!bus_->send(frame)) return SubscriptionSendError::BUS_SEND_FAILED;
        return SubscriptionSendError::NONE;
    }

    void NodeClient::remove_local_subscriptions(Category c, Topic t) {
        for (auto it = subs_.begin(); it != subs_.end();) {
            if (it->category == c && it->topic == t) {
                it = subs_.erase(it);
            } else {
                ++it;
            }
        }
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