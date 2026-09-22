#include "dc_bus_manager.hpp"

namespace dc {
    IBus& BusManager::add(std::unique_ptr<IBus> bus) {
        entries_.push_back(Entry{std::move(bus), std::nullopt});
        return *entries_.back().bus;
    }

    IBus* BusManager::get(BusType type) {
        for (auto& entry : entries_) if (entry.bus->type() == type) return entry.bus.get();
        return nullptr;
    }

    const IBus* BusManager::get(BusType type) const {
        for (auto& entry : entries_) if (entry.bus->type() == type) return entry.bus.get();
        return nullptr;
    }

    BusManager::AttachReceiverError BusManager::attach_receiver(
        IBus& bus, IFrameSink& sink) {
        for (auto& entry : entries_) {
            if (entry.bus.get() != &bus) continue;
            if (entry.monitor.has_value()) {
                return AttachReceiverError::ALREADY_ATTACHED;
            }
            entry.monitor.emplace(bus, sink);
            return AttachReceiverError::NONE;
        }
        return AttachReceiverError::UNKNOWN_BUS;
    }

    bool BusManager::poll_receivers_once() {
        bool received_any = false;
        for (auto& entry : entries_) {
            if (!entry.monitor.has_value()) continue;
            if (entry.monitor->poll_once()) received_any = true;
        }
        return received_any;
    }

    bool BusManager::start_all() {
        bool ok = true;
        for (auto& entry : entries_) ok = entry.bus->start() && ok;
        return ok;
    }

    void BusManager::stop_all() {
        for (auto& entry : entries_) entry.bus->stop();
    }

    bool BusManager::start(BusType type) {
        auto* b = get(type);
        return b ? b->start() : false;
    }

    void BusManager::stop(BusType type) {
        auto* b = get(type);
        if (b) b->stop();
    }

    std::vector<BusManager::Snapshot> BusManager::snapshot() const {
        std::vector<Snapshot> out;
        out.reserve(entries_.size());
        for (const auto& entry : entries_) {
            out.push_back(Snapshot{entry.bus->type(), entry.bus->state(), entry.bus->stats()});
        }
        return out;
    }

}