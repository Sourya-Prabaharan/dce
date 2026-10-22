# Verification record · October 5, 2026

## Native build and tests

- Apple M3 / 8 logical CPUs / 8 GiB RAM, Apple Clang 17.0.0, CMake Release.
- `ctest --test-dir build --output-on-failure`: unit and integration passed, 6.74 seconds total. See `results/tests.log`.
- `BUILD_DIR=build-quickstart sh scripts/quickstart.sh` from a previously nonexistent build directory: configuration, compilation, both tests (6.85 seconds), generator, real CLI, three-worker prime and CSV checks, and acknowledged busy-worker SIGKILL recovery all passed. The additional quickstart recovery was observed at 408.476 ms. See `results/quickstart.log`.
- A 120-job batch returned the known count of 1,229 primes below 10,000 for every job, with no failed jobs. Recorded elapsed time 0.0372041 seconds for this deliberately small scheduling stress workload; this is **not** the compute scaling benchmark. See `results/batch.csv`.
- Numerical unit checks include the known count of 9,592 primes below 100,000 and manually derived CSV statistics. The serial reference shares the kernel but bypasses partitioning/networking.
- Real coordinator restart caused the existing worker processes to reconnect. SIGSTOP/SIGCONT caused a worker to lose its old session and register a new session. Unit tests separately reject duplicate and stale attempt results.

## Repeated measurements

`results/benchmark.csv` has 30 timed rows: two workloads × three worker counts × five repetitions. All matched their serial references. One complete warmup per workload/configuration was excluded. The prime benchmark uses 8 million integers; CSV uses 1 million values. Every worker has one compute thread; all processes share the host. `results/hardware.txt`, `results/summary.md`, and the README describe the methodology and limitations. No alternative scheduler baseline was measured.

`results/failure-kill.csv`: five SIGKILL trials, all correct and below five seconds; maximum 402.660 ms. `results/failure-heartbeat.csv`: three SIGSTOP trials, all correct and below five seconds; maximum 2,062.730 ms. Separate CTest and clean-quickstart trials are checks, not silently mixed into this repeated measurement series. No trial in the repeated series was discarded.

## Docker / Linux correctness

The Docker CLI existed outside the default PATH. Verification used `/Applications/Docker.app/Contents/Resources/bin/docker`, with that directory on PATH for credential helper discovery. Docker Desktop 4.57.0 / Engine 29.1.3, Linux/arm64 containers, GCC 12.2.0. Existing unrelated user containers were left untouched.

Commands:

```sh
docker compose -p dce-verification build
docker compose -p dce-verification up -d --wait
docker compose -p dce-verification exec -T coordinator dce-cli workers
docker compose -p dce-verification exec -T coordinator dce-cli demo --end 2000000
docker compose -p dce-verification exec -T coordinator dce-cli demo --type csv --file numbers.csv
```

All images built, Linux unit tests passed during image construction, and all four containers started. Saved job task lists prove that all three worker sessions participated in both workloads:

- `results/docker-primes.json`: count 148,933.
- `results/docker-csv.json`: count 20,000, sum −10,000, min −500, max 499, mean ≈ −0.5, variance 83,333.25.
- `results/docker-workers.json`: initial worker sessions.

Then the native C++ CLI submitted [0,20,000,000) with three partitions to the container API. `results/docker-before-kill.json` records all three tasks running and acknowledged. `docker compose -p dce-verification kill -s SIGKILL worker-1` terminated one busy container. The CLI waited for completion; `results/docker-recovered.json` shows one task with two attempts and the correct count 1,270,607. `docker compose ... start worker-1` manually restarted that container; `results/docker-restarted-workers.json` records three live sessions and an additional expired worker-1 session. `results/docker-runtime.log` contains assignments, loss, requeue, and completion events. The test stack was stopped with `docker compose -p dce-verification down` after verification.

Docker image construction experienced transient DNS failures that `curl --retry` recovered from. See `results/docker-build.log`. No repeated Docker performance benchmark was run. Native Linux on a separate physical machine was not tested.

## Browser

The in-app browser loaded the actual coordinator-served dashboard, displayed three healthy workers, submitted a prime job (78,498 primes below one million), and submitted the generated numeric CSV (20,000 values). Both completed. Saved native responses are in `results/dashboard-jobs.json`. The browser was then reloaded against the container deployment; it displayed completed jobs and the expired/restarted worker sessions. `results/dashboard.png` is a viewport screenshot of that verified state. Narrow layouts scroll their tables horizontally.

## Sanitizer limitation

A Debug build with `-fsanitize=address,undefined -fno-omit-frame-pointer` compiled. Running it on this Mac stalled before the application's `main()`. A one-second process sample showed Apple ASan initialization recursively entering its own allocator from `dyld_shared_cache_iterate_text_swift`, then spinning in `StaticSpinMutex::LockSlow`. The run was terminated; **no sanitizer pass or sanitizer-clean claim is made**. Evidence is saved in `results/sanitizer-startup-sample.txt` and `results/sanitizers.log`. Normal native and Linux release checks passed independently.

## Repository migration verification · October 22, 2026

A new `build-migration` directory was configured and compiled in Release mode after attaching the GitHub repository history. Both CTest suites passed: unit 0.70 s, integration 5.87 s, total 6.58 s. The integration test rechecked three-worker prime/CSV execution, the actual CLI, 120 correct completed jobs, invalid-input retries, coordinator restart/reconnect, SIGKILL recovery, and heartbeat-silent recovery. Full output is retained in `results/migration-tests.log`. These are new correctness checks; the October 5 benchmark series remains separately dated.

The active Python engine and obsolete Python setup documentation were replaced, with the original implementation retained at commit `3340626`. The GitHub Pages overview now describes the C++ engine and links to actual measurements, rather than presenting a browser simulation as engine execution. A Linux/macOS C++ CI workflow was added; local success does not imply that a remote Actions run has passed.
