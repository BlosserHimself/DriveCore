#pragma once
#include <memory>
#include <vector>
#include <optional>

#include "dc_bus.hpp"
#include "dc_bus_monitor.hpp"
#include "dc_dispatcher.hpp"
#include "dc_frame_sink.hpp"

namespace dc {
    class BusManager {
        public:
            enum class AttachReceiverError : uint8_t {
                NONE,
                UNKNOWN_BUS,
                ALREADY_ATTACHED,
            };

            enum class AttachTransmitterError : uint8_t {
                NONE,
                UNKNOWN_BUS,
                ALREADY_ATTACHED,
            };

            // Ownership: BusManager owns bus objects. Returns a reference to
            // the bus instance now owned by this manager.
            IBus& add(std::unique_ptr<IBus> bus);

            // Find bus by type (OEM / DRIVECORE / CLUSTER)
            IBus* get(BusType type);
            const IBus* get(BusType type) const;

            // Attaches RX-only monitoring to a bus already owned by this
            // manager. `bus` must be a reference previously returned by
            // add() on this same BusManager. `sink` remains externally
            // owned and must outlive the attached monitor.
            AttachReceiverError attach_receiver(IBus& bus, IFrameSink& sink);

            // Attaches TX-only dispatch infrastructure to a bus already
            // owned by this manager. `bus` must be a reference previously
            // returned by add() on this same BusManager.
            AttachTransmitterError attach_transmitter(IBus& bus);

            // Returns the Dispatcher attached to `bus`, or nullptr if `bus`
            // is unknown to this manager or has no attached Dispatcher.
            Dispatcher* dispatcher(IBus& bus);

            // One RX service round: each attached monitor gets exactly one
            // poll_once() opportunity. Returns true if any monitor handled
            // a frame. Entries without an attached monitor are skipped.
            bool poll_receivers_once();

            // One TX service round: each attached Dispatcher gets exactly
            // one service_once() opportunity. Returns true if any Dispatcher
            // sent a Frame. Entries without an attached Dispatcher are
            // skipped.
            bool service_transmitters_once();

            // Start/stop all buses (or individual by type)
            bool start_all();
            void stop_all();

            bool start(BusType type);
            void stop(BusType type);

            // Pump all buses
            template <typename Fn>
            void pump(Fn&& on_frame) {
                Frame f{};
                for (auto& entry : entries_) {
                    IBus& b = *entry.bus;
                    if (b.state() != BusState::RUNNING) continue;
                    while (b.poll(f)) {
                        on_frame(b, f);
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
            // Declaration order matters: monitor/dispatcher must be
            // destroyed before the bus they reference.
            struct Entry {
                std::unique_ptr<IBus> bus;
                std::optional<BusMonitor> monitor;
                std::optional<Dispatcher> dispatcher;
            };

            std::vector<Entry> entries_;
    };
}
