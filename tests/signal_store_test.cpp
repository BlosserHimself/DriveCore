#include <array>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <unordered_map>

#include "dc_signal_key.hpp"
#include "dc_signal_store.hpp"
#include "dc_topics.hpp"

namespace {
    constexpr dc::Category sensor = dc::category::SENSOR;
    constexpr dc::Topic rpm = dc::topic::sensor::RPM;
    constexpr dc::Topic coolant = dc::topic::sensor::COOLANT;

    // Proof A: fixed, compile-time-known signal set. No heap, no
    // unordered_map, single supported value type. Representative of a
    // constrained, MCU-style application store.
    class TinySignalStore {
        public:
            template <typename ValueType>
            void write(const dc::SignalKey& key, ValueType value)
                requires std::same_as<ValueType, uint16_t> {
                Slot* slot = find(key);
                if (!slot) return; // unknown key: fixed signal set by design
                slot->value = value;
                slot->populated = true;
            }

            template <typename ValueType>
            ValueType read(const dc::SignalKey& key, ValueType fallback) const
                requires std::same_as<ValueType, uint16_t> {
                const Slot* slot = find(key);
                if (!slot || !slot->populated) return fallback;
                return slot->value;
            }

            bool has(const dc::SignalKey& key) const {
                const Slot* slot = find(key);
                return slot && slot->populated;
            }

        private:
            struct Slot {
                dc::SignalKey key;
                uint16_t value = 0;
                bool populated = false;
            };

            Slot* find(const dc::SignalKey& key) {
                for (auto& slot : slots_) {
                    if (slot.key == key) return &slot;
                }
                return nullptr;
            }

            const Slot* find(const dc::SignalKey& key) const {
                for (const auto& slot : slots_) {
                    if (slot.key == key) return &slot;
                }
                return nullptr;
            }

            std::array<Slot, 2> slots_{{
                Slot{dc::SignalKey{sensor, rpm}, 0, false},
                Slot{dc::SignalKey{sensor, coolant}, 0, false},
            }};
    };

    // Proof B: dynamic storage, multiple logical signals and value types in
    // one instance. Demonstrates the contract does not force Proof A's
    // fixed/static layout.
    //
    // write/read are templates constrained to exactly {uint16_t, uint32_t}
    // rather than plain overloads of those two types: plain overloads would
    // still reject a third type in practice (both candidates are equally
    // ranked conversions, so the call is ambiguous), but that safety would be
    // an accident of picking two mutually-ambiguous widths, not a declared
    // guarantee. The explicit constraint makes the supported-type set exact
    // regardless of which widths happen to be present.
    class RichSignalStore {
        public:
            template <typename ValueType>
            void write(const dc::SignalKey& key, ValueType value)
                requires (std::same_as<ValueType, uint16_t> ||
                          std::same_as<ValueType, uint32_t>) {
                if constexpr (std::same_as<ValueType, uint16_t>) {
                    entries16_[key] = value;
                } else {
                    entries32_[key] = value;
                }
            }

            template <typename ValueType>
            ValueType read(const dc::SignalKey& key, ValueType fallback) const
                requires (std::same_as<ValueType, uint16_t> ||
                          std::same_as<ValueType, uint32_t>) {
                if constexpr (std::same_as<ValueType, uint16_t>) {
                    auto it = entries16_.find(key);
                    return it == entries16_.end() ? fallback : it->second;
                } else {
                    auto it = entries32_.find(key);
                    return it == entries32_.end() ? fallback : it->second;
                }
            }

            bool has(const dc::SignalKey& key) const {
                return entries16_.count(key) != 0 || entries32_.count(key) != 0;
            }

        private:
            std::unordered_map<dc::SignalKey, uint16_t, dc::SignalKeyHash> entries16_;
            std::unordered_map<dc::SignalKey, uint32_t, dc::SignalKeyHash> entries32_;
    };

    // Proof C: intentionally one-directional stores, used only to prove the
    // capability concepts reject a store missing that capability.
    class WriteOnlySignalStore {
        public:
            void write(const dc::SignalKey&, uint16_t) {}
    };

    class ReadOnlySignalStore {
        public:
            uint16_t read(const dc::SignalKey&, uint16_t fallback) const {
                return fallback;
            }
    };

    static_assert(dc::SignalStore<TinySignalStore, uint16_t>);
    static_assert(dc::SignalStateQueryable<TinySignalStore>);
    static_assert(!dc::SignalWritable<TinySignalStore, uint32_t>);
    static_assert(!dc::SignalReadable<TinySignalStore, uint32_t>);
    // Same-width, different-signedness is the realistic accidental-acceptance
    // case (unlike a width mismatch, it cannot be ruled out by overload
    // ambiguity alone) — confirm the same_as constraint still rejects it.
    static_assert(!dc::SignalWritable<TinySignalStore, int16_t>);
    static_assert(!dc::SignalReadable<TinySignalStore, int16_t>);

    static_assert(dc::SignalStore<RichSignalStore, uint16_t>);
    static_assert(dc::SignalStore<RichSignalStore, uint32_t>);
    static_assert(dc::SignalStateQueryable<RichSignalStore>);
    static_assert(!dc::SignalWritable<RichSignalStore, uint8_t>);
    static_assert(!dc::SignalReadable<RichSignalStore, uint8_t>);
    static_assert(!dc::SignalWritable<RichSignalStore, int16_t>);
    static_assert(!dc::SignalReadable<RichSignalStore, int16_t>);
    static_assert(!dc::SignalWritable<RichSignalStore, int32_t>);
    static_assert(!dc::SignalReadable<RichSignalStore, int32_t>);
    static_assert(!dc::SignalWritable<RichSignalStore, int64_t>);
    static_assert(!dc::SignalReadable<RichSignalStore, int64_t>);
    static_assert(!dc::SignalWritable<RichSignalStore, uint64_t>);
    static_assert(!dc::SignalReadable<RichSignalStore, uint64_t>);
    static_assert(!dc::SignalWritable<RichSignalStore, bool>);
    static_assert(!dc::SignalReadable<RichSignalStore, bool>);

    static_assert(dc::SignalWritable<WriteOnlySignalStore, uint16_t>);
    static_assert(!dc::SignalReadable<WriteOnlySignalStore, uint16_t>);

    static_assert(dc::SignalReadable<ReadOnlySignalStore, uint16_t>);
    static_assert(!dc::SignalWritable<ReadOnlySignalStore, uint16_t>);
    static_assert(!dc::SignalStateQueryable<ReadOnlySignalStore>);

    void test_tiny_store_write_read_state() {
        TinySignalStore store;
        const dc::SignalKey rpm_key{sensor, rpm};

        assert(!store.has(rpm_key));
        assert(store.read(rpm_key, static_cast<uint16_t>(0)) == 0);

        store.write(rpm_key, static_cast<uint16_t>(4200));

        assert(store.has(rpm_key));
        assert(store.read(rpm_key, static_cast<uint16_t>(0)) == 4200);
    }

    void test_tiny_store_ignores_unknown_signal() {
        TinySignalStore store;
        const dc::SignalKey unknown_key{
            dc::category::CLIMATE, dc::topic::climate::DRIVER_SEAT_HEAT};

        store.write(unknown_key, static_cast<uint16_t>(1));

        assert(!store.has(unknown_key));
        assert(store.read(unknown_key, static_cast<uint16_t>(7)) == 7);
    }

    void test_rich_store_supports_multiple_types_and_signals() {
        RichSignalStore store;
        const dc::SignalKey rpm_key{sensor, rpm};
        const dc::SignalKey coolant_key{sensor, coolant};

        store.write(rpm_key, static_cast<uint16_t>(4200));
        store.write(coolant_key, static_cast<uint32_t>(90));

        assert(store.read(rpm_key, static_cast<uint16_t>(0)) == 4200);
        assert(store.read(coolant_key, static_cast<uint32_t>(0)) == 90);
        assert(store.has(rpm_key));
        assert(store.has(coolant_key));
    }

    void test_distinct_keys_remain_distinct_in_store() {
        RichSignalStore store;
        const dc::SignalKey rpm_key{sensor, rpm};
        const dc::SignalKey coolant_key{sensor, coolant};

        store.write(rpm_key, static_cast<uint16_t>(1));
        store.write(coolant_key, static_cast<uint16_t>(2));

        assert(store.read(rpm_key, static_cast<uint16_t>(0)) == 1);
        assert(store.read(coolant_key, static_cast<uint16_t>(0)) == 2);
    }
}

int main() {
    test_tiny_store_write_read_state();
    test_tiny_store_ignores_unknown_signal();
    test_rich_store_supports_multiple_types_and_signals();
    test_distinct_keys_remain_distinct_in_store();
}
