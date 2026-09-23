// DriveCore example: receive_only
//
// Demonstrates the smallest useful receive-only application composition:
//
//     FakeBus/FakeCanMedium -> BusMonitor -> application-owned IFrameSink
//         -> application decoding -> application-owned signal store
//         -> SignalHandle<Store, uint16_t>
//
// This is ONE application composition, not a mandatory DriveCore RX
// pipeline. See README.md in this directory for what this example does and
// does not demonstrate.

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>

#include "dc_bus_monitor.hpp"
#include "dc_fake_bus.hpp"
#include "dc_frame_codec.hpp"
#include "dc_frame_sink.hpp"
#include "dc_signal_handle.hpp"
#include "dc_signal_key.hpp"
#include "dc_topics.hpp"

namespace {
    constexpr dc::SignalKey rpm_key{dc::category::SENSOR, dc::topic::sensor::RPM};

    // Application-owned store for exactly one logical signal. Deliberately
    // minimal: a real application chooses its own store shape and capacity.
    class RpmStore {
        public:
            void write(const dc::SignalKey& key, uint16_t value) {
                if (key != rpm_key) return; // this store only holds RPM
                value_ = value;
                populated_ = true;
            }

            uint16_t read(const dc::SignalKey& key, uint16_t fallback) const {
                if (key != rpm_key || !populated_) return fallback;
                return value_;
            }

        private:
            uint16_t value_ = 0;
            bool populated_ = false;
    };

    static_assert(dc::SignalReadable<RpmStore, uint16_t>);
    static_assert(dc::SignalWritable<RpmStore, uint16_t>);

    // Application-owned receive sink: recognizes and decodes exactly one
    // DATA signal (RPM). Not a general-purpose decoder.
    class RpmFrameSink final : public dc::IFrameSink {
        public:
            explicit RpmFrameSink(RpmStore& store) : store_(store) {}

            void on_frame(const dc::Frame& frame) override {
                dc::DataWireId wire_id{0, 0, 0, 0};
                dc::FramePayload payload{};
                if (dc::decode_data_frame(frame, wire_id, payload) !=
                    dc::FrameCodecError::NONE) {
                    return;
                }
                if (wire_id.category != rpm_key.category ||
                    wire_id.topic != rpm_key.topic) {
                    return;
                }
                if (payload.length != 2) return;

                const uint16_t value = static_cast<uint16_t>(payload.data[0]) |
                    (static_cast<uint16_t>(payload.data[1]) << 8);
                store_.write(rpm_key, value);
            }

        private:
            RpmStore& store_;
    };

    // Peer/test-stimulus side: encodes and sends the one DATA frame this
    // example receives. The receive-only application itself never sends.
    bool send_rpm_frame(dc::IBus& peer, uint16_t rpm_value) {
        const dc::DataWireId wire_id{0, rpm_key.category, rpm_key.topic, 0};
        dc::FramePayload payload{};
        payload.length = 2;
        payload.data[0] = static_cast<uint8_t>(rpm_value & 0xFF);
        payload.data[1] = static_cast<uint8_t>((rpm_value >> 8) & 0xFF);

        dc::Frame frame{};
        if (dc::encode_data_frame(wire_id, payload, frame) !=
            dc::FrameCodecError::NONE) {
            return false;
        }
        return peer.send(frame);
    }
}

int main() {
    auto medium = std::make_shared<dc::FakeCanMedium>();
    dc::FakeBus peer(medium);     // transmitting stimulus, not the application
    dc::FakeBus app_bus(medium);  // the receive-only application's bus

    assert(peer.start());
    assert(app_bus.start());

    RpmStore store;
    RpmFrameSink sink(store);
    dc::BusMonitor monitor(app_bus, sink);

    assert(send_rpm_frame(peer, 3500));

    // One frame was sent; one poll is sufficient to receive and decode it.
    assert(monitor.poll_once());

    dc::SignalHandle<RpmStore, uint16_t> rpm{store, rpm_key};
    const uint16_t value = rpm.get(0);

    assert(value == 3500);
    std::printf("RPM == %u\n", static_cast<unsigned>(value));
    return 0;
}
