# Interview walkthrough

## A concise introduction

“I implemented a C++20 coordinator/worker engine with a framed TCP protocol. The coordinator partitions deterministic jobs, tracks worker slots, and distributes tasks to the least-loaded worker relative to capacity. Workers run computation separately from networking. Session and attempt IDs make retries safe: execution is at least once, but only the current attempt can publish a result. I tested three local worker processes, batch submission, and real failures, then measured scaling rather than assuming it.”

Use the measured tables in the README to attach numbers to this description. Describe logical nodes accurately; this test did not involve three physical computers.

## Walk the code in this order

1. `compute.cpp`: show the pure task interface (`JSON task → JSON partial result`). Explain half-open prime ranges and byte-aligned CSV line ownership. Deterministic, side-effect-free kernels make replay acceptable. Parallel Welford aggregation carries sufficient statistics rather than averaging partition means incorrectly.
2. `protocol.cpp`: a four-byte big-endian length disambiguates messages in a byte stream. `async_read` gathers a full header/body; the write queue prevents frame interleaving and owns buffers through completion. Discuss invalid lengths, partial delivery, bounded queues, and EOF.
3. `scheduler.cpp`: trace submit → queue → least load/capacity → assignment. A mutex protects queue and state together, so a task cannot be assigned twice by concurrent dispatchers. The actual coordinator uses one event loop; the mutex makes the scheduler independently usable and directly testable.
4. `worker.cpp`: explain the fixed thread pool and why an infinite compute loop cannot delay the event loop's heartbeat timer. Outstanding work is capped at registered capacity. Session cancellation and waiting for old work before reconnect keep repeated disconnects from creating an unbounded pool queue.
5. `Scheduler::finish`: show the state/session/attempt acceptance predicate. Then point at tests for duplicate, stale, and malformed replies. Explain why validation happens before releasing slot ownership: otherwise a malformed reply could orphan its task.
6. `tests/integration.cpp`: show fork/exec of real processes, independent API observations, SIGKILL/SIGSTOP, checked aggregates, every failure trial, and 120 jobs with known prime counts. Contrast this with a unit test that only calls a simulated failure function.

## Questions and honest answers

**Why not exactly once?** A worker can finish but lose its connection before its result is accepted. A retry can execute the same task again. Exactly-once external effects would need transactional coupling or idempotency keys in the side-effecting system. Here, deterministic computation is safe to repeat and the coordinator accepts at most one authoritative result per task.

**What fails when the coordinator crashes?** Jobs and results are lost. Workers reconnect but cannot reconstruct job state. A future design could persist job creation, assignment epochs, and accepted results in a write-ahead log, then replay it before accepting connections. Coordinator HA and consensus are outside this project's scope.

**Why both heartbeat timeout and execution leases?** A dead process can close its socket immediately. A frozen or unreachable process may leave the connection looking alive, so heartbeats detect that failure. A process can still send heartbeats while a computation is stuck; the hard execution lease bounds that case. ACK deadlines also detect non-started assignments. A lease expiry evicts the session, simplifying resource accounting at the cost of retrying other tasks on that worker.

**Why normalize load?** One outstanding task saturates a one-slot worker, but consumes only one quarter of a four-slot worker. The scheduler compares these fractions and never exceeds capacity. It does not predict task runtimes or hardware speed. Show the slot utilization metrics, but do not claim idle-time reduction without comparing to a measured baseline scheduler.

**What limits recovery latency?** Detection, the 100 ms sweep, queue position, healthy slot availability, scheduling and network transit. The failure experiment leaves healthy workers available; reported timing includes waiting for a slot and client polling. The two-second heartbeat setting is not a universal recovery guarantee.

**Why is CSV scaling weaker?** Parsing, file access, allocation, per-task overhead and shared-host resources compete. Short jobs also amplify polling overhead. The CSV benchmark reads a warm shared filesystem cache. Do not attribute a precise bottleneck without profiling; these are plausible explanations, not measured CPU profiles.

**Are serial tests independent?** Serial execution uses the same compute kernel over one whole range/file, validating partition and aggregation behavior. Unit tests additionally compare primes with known counts and CSV with manually derived statistics, catching errors a shared kernel alone could hide.

**Why not a sieve or Arrow?** Trial division and a strict numeric CSV parser keep the implementation compact and explainable. They perform actual useful arithmetic, but aren't intended to beat specialized numerical engines. Faster kernels could make network and orchestration overhead proportionally larger.

**How would you grow this?** First add persistent jobs/results, authentication, bounded result pagination, immutable input fingerprints, and richer admission control. Then profile before changing the single event loop. Scheduling is currently a linear scan of retained worker sessions per dispatch and FIFO has no inter-job fairness. Larger installations need session pruning, priority/fair queues, backpressure and cancellation, possibly data locality and bandwidth-aware placement.

## Suggested resume bullets

Use only the numbers in the saved results; see the README's measured bullets. Always include “local worker processes” or “logical nodes” if describing this demonstration. Do not claim physical multi-node speedup, exactly-once execution, coordinator failover, or reduced idle time.
