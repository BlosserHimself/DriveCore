#pragma once
#include <concepts>

#include "dc_signal_key.hpp"

namespace dc {
    // Can `Store` accept a value of type T for a logical signal? Imposes no
    // storage layout, allocation strategy, or concurrency model.
    template <typename Store, typename T>
    concept SignalWritable = requires(Store& store, const SignalKey& key, T value) {
        store.write(key, value);
    };

    // Can `Store` return a value of type T for a logical signal, falling
    // back to a caller-supplied default when no value has ever been written?
    template <typename Store, typename T>
    concept SignalReadable = requires(const Store& store, const SignalKey& key, T fallback) {
        { store.read(key, fallback) } -> std::same_as<T>;
    };

    // Can `Store` report whether a logical signal currently holds a value?
    // Deliberately independent of any particular value type.
    template <typename Store>
    concept SignalStateQueryable = requires(const Store& store, const SignalKey& key) {
        { store.has(key) } -> std::same_as<bool>;
    };

    // Convenience: full read+write capability for one logical value type.
    // Not every store needs to satisfy this for every type it stores.
    template <typename Store, typename T>
    concept SignalStore = SignalReadable<Store, T> && SignalWritable<Store, T>;
}
