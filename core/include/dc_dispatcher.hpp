#pragma once
#include <optional>

#include "dc_bus.hpp"
#include "dc_frame.hpp"

namespace dc {
    // Transport-side TX boundary: accepts complete Frames and gives them a
    // physical send opportunity. Knows nothing about DriveCore semantics
    // (SignalKey, DATA/COMMAND/SUBSCRIPTION, categories, topics).
    class Dispatcher {
        public:
            enum class SubmitResult : uint8_t {
                ACCEPTED,
                PENDING_FULL,
            };

            enum class ServiceResult : uint8_t {
                NOTHING_PENDING,
                SENT,
                SEND_FAILED,
            };

            explicit Dispatcher(IBus& bus) : bus_(bus) {}

            // Marks `frame` ready for transmission. Does not itself call
            // IBus::send(); a future service_once() owns that opportunity.
            SubmitResult submit(const Frame& frame) {
                if (pending_.has_value()) return SubmitResult::PENDING_FULL;
                pending_ = frame;
                return SubmitResult::ACCEPTED;
            }

            // At most one physical IBus::send() attempt. On success the
            // pending frame is cleared; on failure it is preserved so a
            // later service_once() can retry it.
            ServiceResult service_once() {
                if (!pending_.has_value()) return ServiceResult::NOTHING_PENDING;
                if (bus_.send(*pending_)) {
                    pending_.reset();
                    return ServiceResult::SENT;
                }
                return ServiceResult::SEND_FAILED;
            }

        private:
            IBus& bus_;
            std::optional<Frame> pending_;
    };
}
