# Distributed Compute Engine

DCE splits compute jobs into smaller tasks and runs them across a group of C++ workers. A coordinator keeps track of available slots, assigns work, and collects the results. If a worker disconnects, its unfinished tasks go back into the queue.

The two workloads are prime counting and numeric CSV analysis. The coordinator, workers, HTTP API, CLI, and tests are C++20. The browser dashboard is plain HTML and JavaScript.

**[Explore the demo](https://sourya-dce.netlify.app/demo.html)** · [Project page](https://sourya-dce.netlify.app/) · [Protocol](docs/protocol.md)

The public demo lets you inspect recorded runs from the C++ engine, including a job that recovered after a worker was killed. To submit your own jobs, run the cluster below and open its dashboard.

## Run locally

You'll need a C++20 compiler, CMake 3.20+, `curl`, and `tar`. Tested with Apple Clang 17 on macOS and GCC 12 in Linux containers. The setup script downloads pinned Boost and JSON headers.

```sh
git clone https://github.com/Sourya-Prabaharan/dce.git
cd dce
sh scripts/deps.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
build/dce-cli generate --output data/numbers.csv --rows 10000
sh scripts/local.sh
```

Open **http://127.0.0.1:8080**. In another terminal:

```sh
build/dce-cli demo --end 2000000 --partitions 12
build/dce-cli demo --type csv --file numbers.csv
```

The first job counts 148,933 primes below two million. The second returns count, sum, min, max, mean, and population variance for the generated CSV. `build/dce-cli serial` runs the reference computation without the cluster.

`WORKERS=4 THREADS=2 sh scripts/local.sh` changes the worker count and slots per worker. For containers, run `docker compose up --build -d` after generating the data. Both setups use the same C++ executables. Ctrl-C stops the local cluster; `docker compose down` stops the containers.

## How it works

```text
CLI / browser ── HTTP ── coordinator
                            │
                    queue + worker registry
                            │ TCP / framed JSON
                  ┌─────────┼─────────┐
               worker 1  worker 2  worker 3
```

- The scheduler chooses a worker using outstanding tasks divided by execution slots. It reserves a slot before sending an assignment.
- Workers send a heartbeat every 500 ms. Computation runs in a bounded thread pool, separate from the networking thread.
- A lost connection or missed heartbeat returns unfinished tasks to the queue. Each task gets at most three attempts by default.
- Every assignment has a new attempt ID. Late replies from an old attempt cannot overwrite the accepted result. Execution is at least once.

A worker can reconnect after a connection failure. Restarting a killed process is the job of Docker or the operator. Coordinator state is in memory, so restarting the coordinator loses its jobs.

## What I tested

The tests cover framing and fragmented messages, scheduling limits, result aggregation, stale replies, retries, reconnects, and failures of real worker processes. A batch test completed 120 jobs and checked every result.

For failure testing, the harness kills or pauses a busy worker while two others remain available. All eight repeated trials recovered correctly. The slowest observed reassignment was **2.063 seconds**. Run it with `sh scripts/failure-demo.sh`.

On an Apple M3 with 8 GiB RAM, the mean wall times across five trials per configuration were:

| Workload | 1 worker | 2 workers | 3 workers |
|---|---:|---:|---:|
| Count primes below 8 million | 0.746 s | 0.428 s | 0.280 s |
| Analyze 1 million CSV values | 0.667 s | 0.539 s | 0.454 s |

Each worker used one compute thread. All ran on the same computer, so these are logical nodes sharing a host. The prime workload sped up 2.67×; CSV improved less. The raw timings, including slower trials, are in [results/benchmark.csv](results/benchmark.csv). Slot utilization is recorded too, but there isn't a baseline comparison showing reduced idle time.

Measurements are dated October 5, 2026. The C++ migration was rebuilt and tested on October 22. See the [test record](docs/verification.md) and [reproduction steps](docs/running-and-benchmarks.md).

## Code map

| Location | What's there |
|---|---|
| `src/coordinator.cpp`, `src/scheduler.cpp` | Registration, job state, dispatch, retries, API routes |
| `src/worker.cpp` | Connection lifecycle, heartbeats, computation pool |
| `src/protocol.cpp` | Length-prefixed TCP messages and ordered writes |
| `src/compute.cpp` | Partitioning, computation, and aggregation |
| `src/cli.cpp`, `src/http.cpp` | CLI and HTTP transport |
| `tests/` | Unit tests and the C++ process-based test harness |
| `web/` | Live dashboard served by the coordinator |
| `website/` | Public Netlify demo and recorded results |

## Limits

This is a small systems project. There is no durable job store, coordinator failover, authentication, or TLS. Run the engine on a trusted network. CSV inputs must be identical and unchanged across workers during a job. Only the built-in workloads are accepted; the engine doesn't run uploaded code.

The [protocol notes](docs/protocol.md) explain message fields and state transitions. [Design notes](docs/interview.md) cover the tradeoffs and what I'd change next.
