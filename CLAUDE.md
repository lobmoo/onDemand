# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
source .env                          # Set LD_LIBRARY_PATH for DDS libraries
cd build
cmake .. -DUSE_TXDDS=ON              # or -DUSE_FASTDDS=ON
make -j$(nproc)
```

Output: `build/demo_exec`, `build/tests/`, `build/sample/`

## Tests

```bash
cd build && make test
# Or run individual test binaries directly, e.g.:
./build/tests/fastdds_node_test
```

Test files: `tests/fastdds_node_test.cc`, `common/log/logger_test.cc`, `common/timer_wheel/timer_scheduler_test.cc`

## Code Formatting

```bash
clang-format -i <file>
```

Config: `.clang-format` (LLVM style, 4-space indent)

---

## Architecture

OnDemand is a high-performance pub-sub system for distributing 100,000+ variables across DDS nodes. The key design: subscribers specify per-variable update frequencies, and the publisher only transmits each variable at the minimum required frequency across all subscribers.

### Core Components

**`core/ondemand/on_demand_pub.h/cc`** — Publisher
Manages variable registration, subscription handling, and scheduled data transmission. Maintains a `varIndex_` (hash → `VarMetadata`) and uses a `TimerScheduler` to fire periodic publish tasks grouped by `(bucketIndex, freqMs)`.

**`core/ondemand/on_demand_sub.h/cc`** — Subscriber
Receives variable definitions via DDS, sends subscription requests, receives data into `VarStore`, and fires user callbacks via a separate `TimerScheduler` grouped by `freqMs`. Uses `writeCount` comparison to only invoke callbacks when data has actually changed.

**`core/ondemand/variable_store.h`** — `VarStore`
Lock-free variable storage using Seqlock (atomic sequence counter: odd = writing, even = stable). 64-byte aligned arena. Dirty flag + `ConcurrentQueue` for tracking changed variables. `read_ptr()` provides zero-copy reads via `ScopedPtr`.

**`core/ondemand/on_demand_common.h`** — Shared utilities
`BucketManager`: distributes variables across 20 buckets via `hash % 20`, each bucket maps to a separate DDS topic (`dsf/var/data/transfer/bucket_N`) to parallelize transmission.

**`common/timer_wheel/`** — `TimerScheduler` / `TimerWheel`
8-layer hierarchical time wheel (256 slots/layer), 1ms tick, supports 1ms–multi-day timers. Thread pool (8 threads) executes callbacks. Used by both pub (publish tasks) and sub (user callbacks).

**`common/fastdds_wrapper/` and `common/txdds_wrapper/`** — DDS abstraction
Template wrappers `DDSTopicWriter<T>` / `DDSTopicReader<T>` over FastDDS or TXDDS (selected at CMake time).

### DDS Topics

| Topic | Direction | Message Type | Purpose |
|-------|-----------|--------------|---------|
| `dsf/sys/var/tableDefine` | Pub → Sub | `PubTableDefine` | Broadcast variable definitions (one per bucket) |
| `dsf/message/commandRequest/subTableRegister` | Sub → Pub | `SubTableRegister` | Subscribe / unsubscribe requests |
| `dsf/var/data/transfer/bucket_N` (N=0–19) | Pub → Sub | `TableDataTransfer` | Batched variable data per bucket |

### Frequency Negotiation

`VarMetadata.currentFreq` is always the minimum across all `freqSubs` entries. When a new subscriber requests a faster rate, the publisher reschedules its timer. When the last subscriber at the fastest rate unsubscribes, `currentFreq` steps up (or becomes `0xFFFFFFFF` = no subscribers).

### Scheduler Rebuild

Both pub and sub use a `*Dirty_` atomic flag + a 50ms background scan loop. When dirty, they diff the current timer groups against the desired groups, cancel removed timers, and add new `ScheduleRecurring` timers. This avoids rebuilding on every subscription change.

### IDL Definitions

`core/ondemand/idl/` — DDS message structs. `TableDataTransfer` uses a serialized `Roaring64Map` as the variable hash mask, with `varData` blobs in matching order.

---

## Maintenance Principles (from `.claude/CLAUDE.md`)

This is a **C++17 legacy codebase**. Priorities when modifying:

1. **Memory safety** — RAII over raw pointers; run ASan after refactoring
2. **Performance** — zero-overhead, move semantics, measure before optimizing
3. **Thread safety** — `shared_mutex` for read-heavy maps, `std::atomic` for counters, consistent lock ordering; run TSan after changes
4. **C++17 only** — no C++20/23 features
5. **Incremental refactoring** — one function/class/file at a time; write characterization tests first; don't change public APIs

Sanitizer flags: `-fsanitize=address,thread,undefined`
