#pragma once
#include <memory>
#include <vector>
#include <optional>

#include "dc_bus.hpp"

namespace dc {
    class BusManager {
        public:
            // Ownership: BusManager owns bus objects
            void add(std::unique_ptr<IBus> bus);

            // Find bus by type (OEM / DRIVECORE / CLUSTER)
            IBus* get(BusType type);
            const IBus* get(BusType type) const;

            // Start/stop all buses (or individual by type)
            bool start_all();
            void stop_all();

            bool start(BusType type);
            void stop(BusType type);

            // Pump all buses
            template <typename Fn>
            void pump(Fn&& on_frame) {
                Frame f{};
                for (auto& b : buses_) {
                    if (b->state() != BusState::RUNNING) continue;
                    while (b->poll(f)) {
                        on_frame(*b, f);
                    }
                }
            }

            // Convenience monitoring access
            struct Snapshot {
                BusType type;
                BusState state;
                BusStats stats;
            };

            std::vector<Snapshot> snapshot() const;

        private:
            std::vector<std::unique_ptr<IBus>> buses_;
    };
}