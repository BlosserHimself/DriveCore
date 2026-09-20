#include "dc_fake_bus.hpp"

#include <algorithm>

namespace dc {
    std::shared_ptr<FakeCanMedium::Endpoint> FakeCanMedium::attach() {
        auto endpoint = std::make_shared<FakeCanMedium::Endpoint>();
        endpoints_.push_back(endpoint);
        return endpoint;
    }

    void FakeCanMedium::detach(
        const std::shared_ptr<FakeCanMedium::Endpoint>& endpoint) {
        endpoints_.erase(
            std::remove_if(
                endpoints_.begin(), endpoints_.end(),
                [&endpoint](const std::weak_ptr<FakeCanMedium::Endpoint>& candidate) {
                    const auto locked = candidate.lock();
                    return !locked || locked == endpoint;
                }),
            endpoints_.end());
    }

    void FakeCanMedium::broadcast(
        const std::shared_ptr<FakeCanMedium::Endpoint>& sender,
        const Frame& frame) {
        for (auto endpoints_it = endpoints_.begin();
             endpoints_it != endpoints_.end();) {
            const auto endpoint = endpoints_it->lock();
            if (!endpoint) {
                endpoints_it = endpoints_.erase(endpoints_it);
                continue;
            }

            if (endpoint != sender && endpoint->state == BusState::RUNNING) {
                endpoint->receive_queue.push_back(frame);
            }
            ++endpoints_it;
        }
    }

    FakeBus::FakeBus(std::shared_ptr<FakeCanMedium> medium, BusType type)
        : medium_(std::move(medium)),
          endpoint_(medium_ ? medium_->attach() : nullptr),
          config_{type} {}

    FakeBus::~FakeBus() {
        if (medium_ && endpoint_) {
            medium_->detach(endpoint_);
        }
    }

    BusType FakeBus::type() const {
        return config_.type;
    }

    BusState FakeBus::state() const {
        return endpoint_ ? endpoint_->state : BusState::ERROR;
    }

    const BusConfig& FakeBus::config() const {
        return config_;
    }

    const BusStats& FakeBus::stats() const {
        return stats_;
    }

    bool FakeBus::start() {
        if (!endpoint_) return false;
        endpoint_->state = BusState::RUNNING;
        return true;
    }

    bool FakeBus::stop() {
        if (!endpoint_) return false;
        endpoint_->state = BusState::STOPPED;
        endpoint_->receive_queue.clear();
        return true;
    }

    bool FakeBus::send(const Frame& frame) {
        if (!medium_ || !endpoint_ || endpoint_->state != BusState::RUNNING) {
            return false;
        }

        medium_->broadcast(endpoint_, frame);
        ++stats_.tx_frames;
        return true;
    }

    bool FakeBus::poll(Frame& out_frame) {
        if (!endpoint_ || endpoint_->state != BusState::RUNNING ||
            endpoint_->receive_queue.empty()) {
            return false;
        }

        out_frame = endpoint_->receive_queue.front();
        endpoint_->receive_queue.pop_front();
        ++stats_.rx_frames;
        return true;
    }
}
