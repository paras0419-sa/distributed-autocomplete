# Distributed In-Memory Autocomplete — Design Spec

**Status:** approved draft · **Date:** 2026-04-22 · **Owner:** paras0419@gmail.com
**Implementation language:** C++20

---

## 1. Purpose & scope

A distributed in-memory datastore serving token-prefix autocomplete over
per-tenant corpora (100K–1M entries each) for a multi-tenant BI analytics
platform. Read-only query path. Index content is derived from tenant schema
seeds plus an accumulating history of past BI queries.

**Out of scope (v1):** indexing raw data-dump row values, ML-based ranking,
typo tolerance (scoped for phase-2 behind a feature flag).

## 2. Requirements & SLOs

| Requirement             | Target                                                                                                     |
| ----------------------- | ---------------------------------------------------------------------------------------------------------- |
| Concurrent users (peak) | 500K                                                                                                       |
| Aggregate QPS (assumed) | ~100K–170K                                                                                                 |
| End-to-end p99          | < 20 ms                                                                                                    |
| Serving-node p99 budget | < 8 ms                                                                                                     |
| Tenants                 | 100–1,000, power-law distribution                                                                          |
| Entries per tenant      | 100K–1M                                                                                                    |
| Query semantics         | Token-prefix match; typo tolerance scoped for phase-2                                                      |
| Ranking                 | Static per-entry weight; per-user recency boost as phase-2 hook                                            |
| Tenant isolation        | Strong data isolation; quota-based perf isolation; whales get own shard                                    |
| Availability            | Graceful degradation — autocomplete is optional; client must handle 4xx/5xx/timeout by hiding the dropdown |
| Freshness target        | Near-real-time via mutable overlay; hourly base rebuild                                                    |

## 3. System topology

Four logical services:

1. **Edge Router** — stateless L7 router. Authenticates tenant, hashes
   `tenant_id` → shard, forwards binary request. Not on the index hot path.
2. **Search Serving Node (C++)** — owns a set of tenants; each tenant has an
   Index Slot (base FST + overlay + ranking metadata). Reactor thread model.
3. **Builder (C++ batch)** — stateless; per-tenant scheduled run; reads query
   log + prior artifact, produces new sealed artifact, uploads to artifact
   store.
4. **Query Logger** — async ring buffer in serving node → Kafka (or equivalent)
   → consumed by Builder.

**Sharding:** consistent hashing on `tenant_id`. One tenant lives on one shard
(no cross-shard fan-out). Whales promoted to dedicated shards.
**Replication:** 3-replica, 3-AZ; shared-nothing; no consensus.
**Control plane** (etcd / Consul / Postgres) carries tenant → shard → version
pointers. **Data plane** (S3/blob) carries artifact bytes. Control plane is
**never on the query hot path.**

## 4. Per-tenant index — three layers

### 4.1 Base FST (immutable, mmap'd)

- Finite-state transducer mapping tokens → packed `entry_id`.
- Flat byte array, walked by pointer arithmetic; dominant cost L2 cache.
- Library: wrapped behind an `FstIndex` interface (swappable).
- **v1 bootstrap: double-array trie** behind the same interface; swap to a
  production FST library in Phase 6.

### 4.2 Entry Table + string arena (immutable, mmap'd)

```cpp
struct Entry {
  uint32_t phrase_offset;   // into string arena
  uint16_t phrase_length;
  float    weight;
  uint32_t last_seen_epoch;
  uint8_t  source_flag;     // column | past_query | data_value
  uint8_t  _pad[3];
};
```

- `entry_id` → fixed-size `Entry` (single cache line).
- String arena is a flat blob; phrase read is one `memcpy`.

### 4.3 Mutable overlay (heap, RCU-swap)

- Concurrent hashmap keyed by first K chars of each token.
- **Populated by the serving node** when it processes a `QueryEvent` locally
  (same event that also ships to Kafka, §6.2): if the selected phrase is
  absent from the base FST, insert a new entry; if present, accumulate a
  weight delta. Impression-only events are not inserted; they contribute to
  Kafka-derived stats only.
- Holds all such deltas since the last base swap. At swap time the overlay is
  dropped — contents were either promoted into the new base FST by the
  builder or fell below the weight floor.
- Bounded size; overflow triggers out-of-cycle rebuild.
- **Not persisted** — Kafka is the durable source of truth (§6). On node
  crash, the overlay is lost and the next build absorbs the missed deltas
  from the Kafka event stream.
- Replica divergence is expected between swaps — each replica's overlay
  reflects only the queries it served. Converges at next base-artifact swap.

### 4.4 IndexView & version swap

`IndexView` bundles the three layers plus version. Serving node holds
`std::atomic<std::shared_ptr<IndexView>>` per tenant. Readers snapshot on query
start; writers install a new snapshot at swap; old snapshot dies when last
reader drops its `shared_ptr`.

## 5. Query path

### 5.1 Request shape

```
GET /autocomplete
  tenant_id, user_id, prefix, k (default 10, max 50), max_ms (default 15)
```

Response = `[ { phrase, source, weight }, ... ]`. **Binary wire format**
(FlatBuffers), never JSON on hot path.

### 5.2 Thread model

Reactor:

- 1 acceptor thread
- N IO threads (~cores / 2): `epoll_wait` → read → parse → enqueue on tenant's
  work queue
- Shared worker pool (~cores): pulls tasks, runs pipeline, hands response back
  to IO thread
- Per-tenant queues, shared pool; fairness via WFQ scheduler (§7)
- Zero allocation on hot path: thread-local bump arena + response ring

### 5.3 Pipeline (budget ≤ 8 ms)

1. parse / auth (~100 µs)
2. tokenize + normalize (~100 µs; lowercase ASCII, split on whitespace/punct;
   no stop-word drop, no stemming; `string_view` all the way)
3. candidate retrieval: FST prefix walk + overlay scan, weight-ordered, early
   termination once top-K proven (~1–3 ms)
4. multi-token intersect (shorter side probes longer), top-K heap, freshness
   decay applied (~0.5–2 ms)
5. serialize response (~200 µs)

### 5.4 Deadline enforcement

Every stage checks `deadline`. On breach: return best-effort top-K,
log `deadline_exceeded=true`. Never throw, never hang.

## 6. Build & distribution path

### 6.1 Seed (tenant onboarding)

Seed v0 index from: (a) tenant's schema catalog — column/metric/table names,
(b) curated BI vocabulary (~500 common terms shipped with the binary).
Excludes raw data-value rows. Schema pulled **once** at onboarding; never live
during query.

### 6.2 Query-log capture

Serving worker emits async `QueryEvent {tenant, ts, prefix, returned_ids,
selected_id, selected_phrase, session_hash, source_of_selected}` to two
destinations in parallel: (1) a per-thread ring buffer drained by a log-shipper
thread that batches → Kafka (or equivalent), per-tenant topic, `session_hash`
partitioning; (2) the tenant's **local overlay** for near-real-time ranking
effect (§4.3). Emission is **non-blocking**; ring overflow drops events with
counter (lossy on log, never on query path). Retention: 7 days rolling.

### 6.3 Builder

Per-tenant, scheduled hourly (or triggered by overlay overflow). Stateless.
Inputs: last artifact (contains cumulative ranking stats) + Kafka offset range.
Process:

1. Aggregate events per phrase: impressions, selections, last_seen.
2. Merge into prior stats:
   `weight = log(1 + cumulative_selections) * exp(-(now - last_seen)/τ) + prior`.
3. Drop phrases below weight floor.
4. Absorb schema changes from current catalog snapshot.
5. Sort phrases; build FST + entry table + string arena.
6. Run **sanity suite** (entry count within ±10% of prior, non-empty FST,
   top-N phrases present). Abort + alert on failure.
7. Upload tarball; register version in control plane.

### 6.4 Artifact format

Path: `s3://autocomplete-artifacts/{tenant_id}/v{N}/bundle.tar`.
Contains `base_fst.bin`, `entry_table.bin`, `string_arena.bin`,
`ranking_stats.bin`, `manifest.json`. Manifest fields: `tenant_id`, `version`
(monotonic int), `built_at`, `source_offsets`, `entry_count`, per-file
`sha256`, `schema_version`, `min_server_version`.

### 6.5 Distribution to serving nodes

Push-pull:

1. Builder writes version record to control plane → event emitted.
2. Serving node (subscribed to its tenants' events) stages download in
   `/var/index/{tenant}/v{N}/`, verifies per-file SHA-256, `mmap`s (read-only,
   `MADV_RANDOM`), constructs new `IndexView`, `atomic_store`s pointer, waits
   for old-view readers to drain, `munmap`s + deletes old staging.
3. Failure at any step: rollback, keep prior version, alert.
4. Canary (phase-2): 1-of-3 replicas loads new version first; promote to
   stable after watch window.

### 6.6 Overlay merge

Each successful base swap drops the overlay — its contents are either
promoted into the new base FST or dropped below weight floor. Between swaps,
overlay growth is implicit backpressure; hard cap triggers out-of-cycle
rebuild.

## 7. Tenant isolation & resource control

### 7.1 Memory

- **Artifact size gate at load time:** sum of artifact file sizes ≤
  `tenant.budget_bytes - overlay_headroom`. Refuse load + alert on breach.
- **Overlay quota:** 10–20% of tenant budget. Overflow → trigger rebuild;
  refuse new inserts until rebuild completes (freshness degrades, correctness
  holds).
- **Thread-local arena overflow** is attributed per tenant and counted; no
  per-allocation tracking.

### 7.2 CPU

- Per-tenant work queues; shared worker pool.
- Scheduler: **Weighted Fair Queueing** (virtual-time). Tenant weight = CPU
  share entitlement.
- Per-query deadline (§5.4) is the backstop: admission refuses requests that
  can't start within budget.

### 7.3 Request rate

- Token-bucket per tenant at every edge router. Capacity = 2× steady-state
  QPS. Rejects at edge (never at serving node).
- Per-router buckets (not globally coordinated). Upper bound ~2× global limit
  across N routers; acceptable at our scale.

### 7.4 Whales

Promote tenant to dedicated shard when any holds:

- artifact size > 50% of shared-node budget
- sustained QPS > 10% of shared-node capacity
- SLA tier = enterprise

Promotion = control-plane topology change only; same binary.

### 7.5 Crash isolation

- Replica redundancy (3× shards) is the primary defense.
- C++ hardening: bounds checks, RAII, ASan + UBSan in CI, fuzzing on parsers
  and FST walker.
- `try/catch` boundary wraps every query → 500, not process crash.
- Whales get process isolation via dedicated shards.

### 7.6 Observability

Every metric tenant-labeled (100–1000 tenants × ~30 metrics is manageable):

- RED: request/error rate, latency histogram
- Resource: memory footprint, CPU share used, rate-limit rejections
- Quality: CTR, mean rank of selected result, zero-result rate

Stack: Prometheus scrape, Grafana per-tenant dashboards.

## 8. Failure handling

| Failure class                 | Recovery                                                                                                                                   |
| ----------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| Single replica down           | Router removes from pool (<15s); other 2 absorb; replacement rejoins via cold-start                                                        |
| Whole shard down              | Auto-rollback on bad deploy; standby-AZ spin-up on infra outage                                                                            |
| Artifact corruption           | SHA-256 verify before swap; retry with exp-backoff+jitter; mark version bad after repeated failures; pin to N-1                            |
| Kafka outage                  | Lossy: log shipper drops events past ring capacity; queries continue. Builder runs find no new data; artifacts stable                      |
| Builder crash                 | Stateless resume from last offset; idempotent aggregation                                                                                  |
| Builder produces bad artifact | Sanity suite gate before upload                                                                                                            |
| Control-plane outage          | Serving nodes cache `tenant → version → path`; query path unaffected; no new version rollouts until recovery                               |
| Cold start                    | Read assigned tenants from control plane; parallel S3 downloads; mark ready only when all tenants loaded (binary readiness)                |
| Poison-pill query             | Input validation at edge (prefix ≤ 128 chars, k ≤ 50, valid UTF-8); per-query deadline + arena cap at node; exception boundary; CI fuzzing |

## 9. Repo layout

```
distributed-autocomplete/
├── CMakeLists.txt
├── conanfile.txt
├── docs/{superpowers/specs/, learning/}
├── include/autocomplete/               # curated public headers
├── libs/                               # each is a static-lib CMake target
│   ├── common/     # Result<T>, Logger, Error, small utils
│   ├── memory/     # ThreadLocalArena, StringArena, pmr adapters
│   ├── index/      # FstIndex, EntryTable, MutableOverlay, IndexView
│   ├── query/      # Tokenizer, QueryExecutor, FixedTopK, Deadline
│   ├── tenant/     # TenantQuota, TokenBucket, WFQ, TenantRegistry
│   ├── artifact/   # Manifest, Downloader, Checksum, VersionSwapper
│   ├── eventlog/   # EventRingBuffer, EventLog, FileEventLog
│   ├── builder/    # aggregation, FST construction glue, sanity suite
│   ├── net/        # IO loop (epoll), codec, admission
│   └── control/    # HealthChecker, GracefulShutdown, control-plane client
├── bin/{serving_node, builder, edge_router}
├── tests/{unit, integration, bench}
├── scripts/                           # load gen, artifact inspector
└── deploy/                            # Dockerfile, k8s (phase-2)
```

Dependency rule: arrows go up, never down. `common` is a leaf dep; executables
in `bin/` are the root.

## 10. Toolchain

- **C++20** (concepts, `std::span`, `std::jthread`, `std::atomic<shared_ptr>`)
- **CMake 3.25+** with target-based style
- **Conan** for third-party dependency management
- **clang-18**, ASan + UBSan in CI from day 1
- **GoogleTest** (unit), **Google Benchmark** (bench)
- Dependencies pinned: flatbuffers, prometheus-cpp, spdlog, FST library TBD
  (v1 substitute: double-array trie), librdkafka (phase-2), aws-sdk-cpp S3
  (phase-2)

## 11. Phased build plan

| Phase | Weeks | Goal                                                     | Exit criterion                                                    |
| ----- | ----- | -------------------------------------------------------- | ----------------------------------------------------------------- |
| 0     | 1     | Repo skeleton, CMake, CI, `common/`                      | `serving_node` binary prints hello; CI green                      |
| 1     | 2–3   | Single-tenant in-memory index (trie placeholder)         | stdin → top-K suggestions over a static corpus                    |
| 2     | 4     | Query pipeline with arena + tokenizer + top-K            | microbench < 1 ms median on 100K corpus                           |
| 3     | 5–6   | Networking, multi-tenant scheduling, rate limit, metrics | `serving_node` handles 10K QPS localhost, Prometheus metrics live |
| 4     | 7–8   | Artifact lifecycle, version swap, health, shutdown       | drop-in new artifact → atomic swap → in-flight drain clean        |
| 5     | 9–10  | Event log (file impl) + builder                          | full loop: event → builder → artifact → swap → new ranking        |
| 6     | 11    | Swap trie → FST, perf polish                             | memory drops 3–5×; p99 holds                                      |
| 7     | 12+   | Kafka, S3, multi-node control plane, chaos + fuzz        | distributed serving across ≥3 nodes                               |

Hands-on C++ exercises for the user (marked 🧠 in the plan) cluster in the
`memory/`, `query/`, and `tenant/` libraries and include at least one exercise
per phase.

## 12. Open items (not blocking v1 start)

- Specific FST library choice for Phase 6 (evaluate `vefst`, Lucene-FST port,
  and writing from scratch as a learning exercise).
- Control-plane substrate decision (etcd vs Consul vs Postgres) — deferred to
  Phase 7.
- Wire format final choice (FlatBuffers vs Cap'n Proto) — benchmark both at
  start of Phase 3.
- Tenant budget tiering values — set by product tier; placeholder 200 MB /
  1 GB / 4 GB.

## 13. References

- Per-section deep-dive topics catalogued in `docs/learning/deep-dives.md`.
- Conversation history of the design discussion is the source of rejected
  alternatives not captured here.
