#include <cassert>
#include <cstdint>

#include "dc_signal_key.hpp"

namespace {
    constexpr dc::Category sensor = dc::category::SENSOR;
    constexpr dc::Topic rpm = dc::topic::sensor::RPM;
    constexpr dc::Topic coolant = dc::topic::sensor::COOLANT;

    void test_equality_and_distinctness() {
        constexpr dc::SignalKey rpm_key{sensor, rpm};
        constexpr dc::SignalKey rpm_key_again{sensor, rpm};
        constexpr dc::SignalKey coolant_key{sensor, coolant};

        static_assert(rpm_key == rpm_key_again);
        static_assert(rpm_key != coolant_key);
        assert(rpm_key == rpm_key_again);
        assert(rpm_key != coolant_key);
    }

    void test_packed_round_trip() {
        constexpr dc::SignalKey rpm_key{sensor, rpm};
        constexpr dc::SignalKey coolant_key{sensor, coolant};

        static_assert(dc::SignalKey::unpack(rpm_key.packed()) == rpm_key);
        static_assert(dc::SignalKey::unpack(coolant_key.packed()) == coolant_key);
        assert(dc::SignalKey::unpack(rpm_key.packed()) == rpm_key);
        assert(dc::SignalKey::unpack(coolant_key.packed()) == coolant_key);
    }

    void test_packed_layout_matches_existing_convention() {
        constexpr dc::SignalKey rpm_key{sensor, rpm};
        static_assert(rpm_key.packed() ==
                      ((static_cast<uint16_t>(sensor) << 8) |
                       static_cast<uint16_t>(rpm)));
    }

    void test_hash_consistent_with_equality() {
        constexpr dc::SignalKey rpm_key{sensor, rpm};
        constexpr dc::SignalKey rpm_key_again{sensor, rpm};

        dc::SignalKeyHash hash;
        assert(hash(rpm_key) == hash(rpm_key_again));
    }
}

int main() {
    test_equality_and_distinctness();
    test_packed_round_trip();
    test_packed_layout_matches_existing_convention();
    test_hash_consistent_with_equality();
}
