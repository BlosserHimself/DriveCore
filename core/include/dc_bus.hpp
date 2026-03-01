#pragma once
#include <cstdint>
#include <string_view>

#include "dc_frame.hpp"

namespace dc {
    enum class BusType : uint8_t {
        OEM = 0,         // Vehicle / Factory CAN
        DRIVECORE = 1,   // DriveCore backbone (private CAN)
        CLUSTER = 2,     // Local cluster CAN (private)
    };

    constexpr std::string_view bust_type_name(BusType t) {
        switch (t) {
            case BusType::OEM:       return "OEM";
            case BusType::DRIVECORE: return "DRIVECORE";
            case BusType::CLUSTER:   return "CLUSTER";
            default:                 return "UNKNOWN";
        }
    }

    struct BusStats {
        uint32_t tx_frames = 0;
        uint32_t rx_frames = 0;
        uint32_t tx_errors = 0;
        uint32_t rx_errors = 0;
        uint32_t last_tx_ms = 0;
        uint32_t last_rx_ms = 0;
    };

    enum class BusState: uint8_t {
        STOPPED = 0,
        RUNNING = 1,
        ERROR   = 2,
    };

    struct BusConfig {
        BusType type;
        uint32_t bitrate = 500000; // Placeholder, platform should change this
        bool extended_ids = true;  // Necessary for 29-bit IDs, do not change
        bool listen_only = false;  // useful for OEM bus sniffing
    };

    class IBus {
        public:
            virtual ~IBus() = default;

            virtual BusType type() const = 0;
            virtual BusState state() const = 0;
            virtual const BusConfig& config() const = 0;
            virtual const BusStats& stats() const = 0;

            // Lifecycle
            virtual bool start() = 0;
            virtual bool stop() = 0;

            // I/O
            virtual bool send(const Frame& frame) = 0;

            // Non-blocking receive. Returns true if frame was read into out_frame
            virtual bool poll(Frame& out_frame) = 0;
    };
}