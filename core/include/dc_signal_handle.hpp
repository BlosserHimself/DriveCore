#pragma once
#include "dc_signal_key.hpp"
#include "dc_signal_store.hpp"

namespace dc {
    // Typed, read-only access path to one logical signal in an
    // application-selected store. Non-owning: the store must outlive the
    // handle. A handle's existence implies nothing about subscription
    // lifetime, requester identity, priority, transport, or wire decoding.
    template <typename Store, typename T>
        requires SignalReadable<Store, T>
    class SignalHandle {
        public:
            constexpr SignalHandle(Store& store, SignalKey key)
                : store_(store), key_(key) {}

            T get(T fallback = T{}) const {
                return store_.read(key_, fallback);
            }

            constexpr const SignalKey& key() const { return key_; }

        private:
            Store& store_;
            SignalKey key_;
    };
}
