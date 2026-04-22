# Deep-Dive Learning Topics

Running list of systems/C++ topics surfaced during the design discussion.
Each item is a task: tick it off once you've read the primary source, written
notes, or built a small experiment.

Format: `- [ ] Topic — one-line hook — (surfaced in: Section N)`

---

## Distributed systems & topology

- [ ] **Consistent hashing with bounded loads** — Google's 2017 paper; explains how to avoid the "hot shard" pathology of vanilla consistent hashing. (Section 1)
- [ ] **No-leader replication for read-only replicas** — why Raft/Paxos are overkill when the data is immutable-per-version and every replica holds the same artifact. (Section 1)
- [ ] **L4 vs L7 load balancing** — latency cost of L7, when connection-level (L4) is enough, and how sticky routing interacts with tenant pinning. (Section 1)
- [ ] **Tenant pinning & hot-tenant mitigation** — dedicated shards for whales, request shedding, per-tenant admission control. (Section 1)

## Data structures for text indexing

- [ ] **FSA/FST construction (Mihov–Maurel incremental algorithm)** — the canonical algorithm for building a minimal FST in a single pass over sorted input. (Section 2)
- [ ] **DAFSA vs FST — when outputs matter** — DAFSA handles set membership; FST handles key→value. Understand the generalization. (Section 2)
- [ ] **Minimal Perfect Hashing (CHD / BDZ / PTHash)** — <3 bits/key for a perfect hash; when it's worth layering on top of an FST vs using the FST's own lookup. (Section 2)

## Systems-C++ & memory

- [ ] **Structure-of-Arrays vs Array-of-Structures** — cache-line economics, when to split a struct into parallel arrays for hot-field access. (Section 2)
- [ ] **`mmap`, page cache, and `madvise()`** — how the OS page cache becomes your index cache; `MADV_RANDOM` vs `MADV_WILLNEED` vs `MADV_SEQUENTIAL`. (Section 2)
- [ ] **Read-mostly concurrency primitives** — RCU, `std::atomic<std::shared_ptr>`, hazard pointers. Trade-offs in cost, API complexity, and memory reclamation. (Section 2)
- [ ] **False sharing & cache-line alignment** — `alignas(64)`, padding between per-thread counters, why naive atomics serialize cores. (Section 2)
- [ ] **Zero-copy serving** — avoiding allocations, buffer reuse, scatter-gather writes, why every memcpy on the hot path shows up in the p99. (Section 2)

---

## How to use this list

1. Pick one topic per implementation milestone — don't binge.
2. For each: find the primary source (paper / man page / reference impl), read it, then write a 5–10 line summary in your own words into `docs/learning/notes/<topic-slug>.md`.
3. Tick the box here; open a small experiment if the topic has a hands-on angle (e.g., write a microbenchmark for false sharing).
4. New topics get appended at the bottom of the relevant section as the design grows.
