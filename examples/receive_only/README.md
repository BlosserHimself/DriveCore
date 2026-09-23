# receive_only

The smallest DriveCore receive-only application composition, demonstrated end
to end: a peer transmits one DATA frame; this application receives, decodes,
and stores it; a `SignalHandle` reads it back.

## What this demonstrates

- `FakeBus`/`FakeCanMedium` as the transport
- `BusMonitor` polling one bus and forwarding raw frames to an
  application-owned `IFrameSink`
- an application-owned sink recognizing and decoding exactly one DATA signal
  (RPM, as `uint16_t`)
- an application-owned signal store satisfying DriveCore's
  `SignalReadable`/`SignalWritable` capability concepts
- `SignalHandle<Store, uint16_t>` reading that store directly, with no
  `NodeClient` involved

## What this deliberately does NOT demonstrate

- subscriptions, `NodeClient`, or `SubscriptionManager`
- COMMAND frames, publication, or a `Dispatcher`
- filtering, routing, scheduling, or freshness/priority policy
- OEM CAN or Cluster CAN topology
- a schema/type registry
- a production `SignalStore` implementation
- a "universal" DriveCore receive pipeline

This is ONE way an application can compose DriveCore's low-level contracts.
Other applications may connect `BusMonitor`, `IFrameSink`, decoding, and
storage differently.

## Flow

```
FakeBus (peer, stimulus)
    -> DATA frame (RPM = 3500)
FakeCanMedium
    -> FakeBus (application, receive-only)
    -> BusMonitor::poll_once()
    -> RpmFrameSink::on_frame()      (application-owned decode)
    -> RpmStore::write()             (application-owned store)
    -> SignalHandle<RpmStore, uint16_t>::get()
    -> 3500
```

The application's own `FakeBus` never calls `send()`; only the peer
transmits.

## Build and run

There is currently no project-wide build system. Compile directly:

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
    -Icore/include -Iprotocol/include \
    examples/receive_only/receive_only.cpp core/src/dc_fake_bus.cpp \
    -o /tmp/receive_only
/tmp/receive_only
```

Expected output:

```
RPM == 3500
```

The example asserts its own correctness (the same convention this
repository's tests use); a non-zero exit code means it failed.
