# Design notes

## Why retries need attempt IDs

A worker can finish a task and lose its connection before the coordinator receives the result. Retrying that task means it may execute twice. The coordinator assigns a new attempt ID and only accepts a result from the current session and attempt. That makes repeated computation safe for these workloads, which have no external side effects.

This would not be enough for a task that charges a card or writes to another service. That service would need its own idempotency or transaction mechanism.

## Keeping heartbeats responsive

Workers have one networking thread and a fixed computation pool. Counting primes happens in the pool, so it cannot hold up the heartbeat timer. The number of outstanding assignments is capped at the worker's registered capacity. On disconnect, computations are canceled cooperatively and drain before the next session registers.

Heartbeats detect a silent worker. An execution lease handles a different case: a process still sending heartbeats while its computation is stuck. Expiring the lease drops the session and retries its tasks. This is simple, but it can repeat healthy work on that same worker.

## Scheduling

The scheduler compares outstanding work with capacity. One task fills a one-slot worker but only uses a quarter of a four-slot worker. This rule avoids assigning beyond the advertised slot count, but it doesn't estimate task duration or machine speed.

The pending queue is FIFO. A large job can delay smaller jobs or retries, and equal-sized prime ranges aren't equally expensive. More partitions help distribute uneven work. A fair queue across jobs and task-time estimates would be useful next steps.

Slot utilization measures how long the coordinator considers a slot occupied. It includes network time and is not a CPU profiler. The current measurements don't establish an improvement over another scheduler.

## CSV aggregation

A task owns complete rows whose starting byte offsets fall inside its assigned range. It skips an initial partial row and finishes its last owned row. This avoids splitting a numeric cell between workers.

Each result carries count, mean, and the sum of squared deviations. The coordinator combines them using the parallel Welford formula. Simply averaging partition means would be wrong when partitions have different numbers of values. Subtracting squared sums can also lose precision for values with a large common offset.

Serial references use the same kernel over the whole input. They catch partition and merge errors. Known prime counts and manually calculated CSV statistics provide checks that don't rely only on that shared code.

## Where this design stops

The coordinator is a single point of failure. Its job state is in memory; workers reconnect after a restart, but the lost jobs do not return. Persistence would need to record submissions and accepted results before acknowledging them, then recover ownership carefully on startup.

Other useful changes would be authentication, immutable input fingerprints, cancellation, and paginated job history. At a larger scale, the worker scan on every dispatch and the single event loop would need profiling. The current three-worker demo doesn't justify adding more infrastructure yet.
