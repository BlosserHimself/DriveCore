#include "dc_signal_cache.hpp"

namespace dc {

    void SignalCache32::update_u16(Category c, Topic t, uint16_t v, uint32_t now_ms) {
        auto& e = entry(c, t);
        e.value.store(static_cast<uint32_t>(v), std::memory_order_relaxed);
        e.last_rx_ms.store(now_ms, std::memory_order_relaxed);
        e.valid.store(1, std::memory_order_release);
    }

    void SignalCache32::update_u32(Category c, Topic t, uint32_t v, uint32_t now_ms) {
        auto& e = entry(c, t);
        e.value.store(v, std::memory_order_relaxed);
        e.last_rx_ms.store(now_ms, std::memory_order_relaxed);
        e.valid.store(1, std::memory_order_release);
    }

    uint16_t SignalCache32::get_u16(Category c, Topic t, uint16_t default_v) const {
        const auto* e = entry_if_exists(c, t);
        if (!e) return default_v;
        if (e->valid.load(std::memory_order_acquire) == 0) return default_v;
        uint32_t v = e->value.load(std::memory_order_relaxed);
        return static_cast<uint16_t>(v & 0xFFFF);
    }

    uint32_t SignalCache32::get_u32(Category c, Topic t, uint32_t default_v) const {
        const auto* e = entry_if_exists(c, t);
        if (!e) return default_v;
        if (e->valid.load(std::memory_order_acquire) == 0) return default_v;
        return e->value.load(std::memory_order_relaxed);
    }

    bool SignalCache32::has_value(Category c, Topic t) const {
        const auto* e = entry_if_exists(c, t);
        return e && (e->valid.load(std::memory_order_acquire) != 0);
    }

    uint32_t SignalCache32::age_ms(Category c, Topic t, uint32_t now_ms) const {
        const auto* e = entry_if_exists(c, t);
        if (!e || e->valid.load(std::memory_order_acquire) == 0) return UINT32_MAX;
        uint32_t last = e->last_rx_ms.load(std::memory_order_relaxed);
        return now_ms >= last ? (now_ms - last) : 0;
    }

    SignalEntry32& SignalCache32::entry(Category c, Topic t) {
        const uint16_t k = SignalKey{c, t}.packed();
        return map_[k]; // creates if missing
    }

    const SignalEntry32* SignalCache32::entry_if_exists(Category c, Topic t) const {
        const uint16_t k = SignalKey{c, t}.packed();
        auto it = map_.find(k);
        if (it == map_.end()) return nullptr;
        return &it->second;
    }

}