#include "dc_bus_manager.hpp"

namespace dc {
    void BusManager::add(std::unique_ptr<IBus> bus) {
        buses_.push_back(std::move(bus));
    }

    IBus* BusManager::get(BusType type) {
        for (auto& b : buses_) if (b->type() == type) return b.get();
        return nullptr;
    }

    const IBus* BusManager::get(BusType type) const {
        for (auto& b : buses_) if (b->type() == type) return b.get();
        return nullptr;
    }

    bool BusManager::start_all() {
        bool ok = true;
        for (auto& b : buses_) ok = b->start() && ok;
        return ok;
    }

    void BusManager::stop_all() {
        for (auto& b : buses_) b->stop();
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
        out.reserve(buses_.size());
        for (const auto& b : buses_) {
            out.push_back(Snapshot{
                .type = b->type(),
                .state = b->state(),
                .stats = b->stats(),
            });
        }
        return out;
    }

}