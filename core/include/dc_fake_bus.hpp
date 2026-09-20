#pragma once
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

#include "dc_bus.hpp"

namespace dc {
    class FakeCanMedium {
        private:
            struct Endpoint {
                std::deque<Frame> receive_queue;
                BusState state = BusState::STOPPED;
            };

            friend class FakeBus;

            std::shared_ptr<Endpoint> attach();
            void detach(const std::shared_ptr<Endpoint>& endpoint);
            void broadcast(const std::shared_ptr<Endpoint>& sender,
                           const Frame& frame);

            std::vector<std::weak_ptr<Endpoint>> endpoints_;
    };

    class FakeBus final : public IBus {
        public:
            explicit FakeBus(std::shared_ptr<FakeCanMedium> medium,
                             BusType type = BusType::DRIVECORE);
            ~FakeBus() override;

            FakeBus(const FakeBus&) = delete;
            FakeBus& operator=(const FakeBus&) = delete;
            FakeBus(FakeBus&&) = delete;
            FakeBus& operator=(FakeBus&&) = delete;

            BusType type() const override;
            BusState state() const override;
            const BusConfig& config() const override;
            const BusStats& stats() const override;

            bool start() override;
            bool stop() override;

            bool send(const Frame& frame) override;
            bool poll(Frame& out_frame) override;

        private:
            std::shared_ptr<FakeCanMedium> medium_;
            std::shared_ptr<FakeCanMedium::Endpoint> endpoint_;
            BusConfig config_;
            BusStats stats_;
    };
}
