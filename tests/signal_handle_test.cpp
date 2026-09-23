#include <cassert>
#include <cstdint>
#include <unordered_map>

#include "dc_signal_handle.hpp"
#include "dc_signal_key.hpp"
#include "dc_signal_store.hpp"
#include "dc_topics.hpp"

namespace {
    constexpr dc::Category sensor = dc::category::SENSOR;
    constexpr dc::Topic rpm = dc::topic::sensor::RPM;
    constexpr dc::Topic coolant = dc::topic::sensor::COOLANT;

    // Test-local store: proves SignalHandle against a store with no
    // connection to NodeClient or SignalCache32.
    class TestSignalStore {
        public:
            void set(const dc::SignalKey& key, uint16_t value) {
                values_[key] = value;
            }

            uint16_t read(const dc::SignalKey& key, uint16_t fallback) const {
                auto it = values_.find(key);
                return it == values_.end() ? fallback : it->second;
            }

        private:
            std::unordered_map<dc::SignalKey, uint16_t, dc::SignalKeyHash> values_;
    };

    // Missing read(): used only to prove a handle cannot be formed over it.
    class WriteOnlyStore {
        public:
            void write(const dc::SignalKey&, uint16_t) {}
    };

    template <typename Store, typename T>
    concept CanFormSignalHandle = requires { typename dc::SignalHandle<Store, T>; };

    static_assert(CanFormSignalHandle<TestSignalStore, uint16_t>);
    static_assert(!CanFormSignalHandle<WriteOnlyStore, uint16_t>);

    void test_handle_identifies_correct_signal_key() {
        TestSignalStore store;
        const dc::SignalKey rpm_key{sensor, rpm};
        dc::SignalHandle<TestSignalStore, uint16_t> handle{store, rpm_key};

        assert(handle.key() == rpm_key);
    }

    void test_fallback_returned_when_unpopulated() {
        TestSignalStore store;
        dc::SignalHandle<TestSignalStore, uint16_t> handle{
            store, dc::SignalKey{sensor, rpm}};

        assert(handle.get(7) == 7);
    }

    void test_populated_value_returned() {
        TestSignalStore store;
        const dc::SignalKey rpm_key{sensor, rpm};
        store.set(rpm_key, 4200);

        dc::SignalHandle<TestSignalStore, uint16_t> handle{store, rpm_key};

        assert(handle.get(0) == 4200);
    }

    void test_distinct_handles_into_same_store_stay_distinct() {
        TestSignalStore store;
        const dc::SignalKey rpm_key{sensor, rpm};
        const dc::SignalKey coolant_key{sensor, coolant};
        store.set(rpm_key, 4200);
        store.set(coolant_key, 90);

        dc::SignalHandle<TestSignalStore, uint16_t> rpm_handle{store, rpm_key};
        dc::SignalHandle<TestSignalStore, uint16_t> coolant_handle{
            store, coolant_key};

        assert(rpm_handle.get(0) == 4200);
        assert(coolant_handle.get(0) == 90);
    }

    void test_handle_does_not_own_or_copy_store_state() {
        TestSignalStore store;
        const dc::SignalKey rpm_key{sensor, rpm};
        store.set(rpm_key, 1000);

        dc::SignalHandle<TestSignalStore, uint16_t> handle{store, rpm_key};
        assert(handle.get(0) == 1000);

        // Mutate the store after handle construction: a copy/snapshot would
        // not observe this, a live non-owning reference does.
        store.set(rpm_key, 2000);
        assert(handle.get(0) == 2000);
    }

    void test_handle_works_without_any_node_client() {
        // No dc::NodeClient is constructed anywhere in this file.
        TestSignalStore store;
        const dc::SignalKey rpm_key{sensor, rpm};
        store.set(rpm_key, 4200);

        dc::SignalHandle<TestSignalStore, uint16_t> handle{store, rpm_key};
        assert(handle.get(0) == 4200);
    }
}

int main() {
    test_handle_identifies_correct_signal_key();
    test_fallback_returned_when_unpopulated();
    test_populated_value_returned();
    test_distinct_handles_into_same_store_stay_distinct();
    test_handle_does_not_own_or_copy_store_state();
    test_handle_works_without_any_node_client();
}
