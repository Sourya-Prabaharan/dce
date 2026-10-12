# Distributed Compute Engine

![Python](https://img.shields.io/badge/Python-3.10%2B-blue)
![Sockets](https://img.shields.io/badge/Networking-TCP%20Sockets-lightgrey)
![Status](https://img.shields.io/badge/Status-Student%20Project-yellow)

A small distributed compute engine that runs Python jobs across worker processes using a master/worker architecture. The project focuses on TCP networking, scheduling, worker heartbeats, basic failure detection, and collecting job output.

## Demo

The quickest demo starts the master, launches a few workers in separate terminals, and submits sample jobs:

```bash
./demo.sh
```

In separate terminals:

```bash
python3 -m worker.worker1
python3 -m worker.worker2
python3 -m worker.worker3
```

Then return to the `demo.sh` terminal and press Enter to submit demo jobs.

## What Problem It Solves

This project shows the core pieces behind a simple distributed job runner:

- accepting work from a client
- tracking available workers
- assigning jobs to idle workers
- detecting missing workers with heartbeats
- collecting output after a worker finishes

It is not meant to be a production system. It is a learning project for distributed systems basics.

## Architecture

```mermaid
flowchart TD
    Client[Client / dcesh] --> Master[Master Node]
    Master --> Queue[Task Queue / Scheduler]
    Queue --> W1[Worker 1]
    Queue --> W2[Worker 2]
    Queue --> W3[Worker 3]
    W1 --> Aggregator[Result Aggregator]
    W2 --> Aggregator
    W3 --> Aggregator
    Aggregator --> Output[Final Output]
    W1 -. heartbeat .-> Master
    W2 -. heartbeat .-> Master
    W3 -. heartbeat .-> Master
    Master -. failure detection .-> Queue
```

The master listens on port `9000`. Workers listen on ports `9101`, `9102`, and `9103`. Workers send heartbeat messages back to the master so the master can mark a worker dead if it stops responding.

## Features

- Master node that accepts job submissions over TCP
- Worker nodes that execute assigned Python jobs in Docker
- FIFO job queue when no worker is available
- Heartbeat-based worker health tracking
- Requeueing for jobs assigned to workers that stop heartbeating
- Simple shell client for submitting jobs and reading logs
- Demo script and sample workload

## Tech Stack

- Python 3
- TCP sockets
- Threads
- Docker for job execution
- Bash demo script

## Build And Run Locally

Requirements:

- Python 3.10 or newer
- Docker, if using `worker/worker1.py`, `worker/worker2.py`, or `worker/worker3.py`
- macOS or Linux shell

Run a quick syntax check:

```bash
python3 -m py_compile master/master.py worker/worker1.py worker/worker2.py worker/worker3.py shell/dcesh.py protocol/*.py
```

Start the master:

```bash
python3 -m master.master
```

Start workers in separate terminals:

```bash
python3 -m worker.worker1
python3 -m worker.worker2
python3 -m worker.worker3
```

Submit a job through the shell:

```bash
python3 -m shell.dcesh
submit examples/distributed_sum.py
status job1
logs job1
```

## Example Demo Commands

```bash
./demo.sh
```

Manual submission without the helper script:

```bash
python3 -m shell.dcesh
```

Inside the shell:

```text
submit examples/distributed_sum.py
status job1
logs job1
```

## Browser preview

The [static website](website/index.html) simulates scheduling and worker events in JavaScript. It does not submit jobs to the Python master. Preview it with `python3 -m http.server 8088 --directory website` and open http://localhost:8088.

## Checks

```bash
python3 -m unittest discover -s tests -v
```

These tests cover queued submissions, connection failure, and completion arriving before the worker's synchronous reply. Docker execution is a separate integration check; Docker must be installed and running for the numbered workers. See [captured state-transition output](docs/test-output.txt).

## Folder Structure

```text
master/       Master node, scheduler, worker health tracking
worker/       Worker processes and Docker-based job execution
protocol/     TCP client/server helpers
shell/        Simple interactive command shell
docker/       Dockerfiles for local/container experiments
docs/         Architecture, demo, and project scope notes
examples/     Sample workloads for demos
```

## Limitations

- Workers are configured with fixed local ports.
- Jobs are assigned as whole Python scripts; one submitted job is not split into smaller chunks yet.
- Job execution currently depends on Docker in the numbered worker scripts.
- The protocol is plain text and intended for learning/debugging.
- There is no authentication or persistent storage. Run only in a trusted local environment.
- State is shared across threads without comprehensive locking; this is not a concurrency-safe production scheduler.
- Heartbeat timeout requeues whole jobs; it does not checkpoint work or provide exactly-once execution.
- Kubernetes deployment manifests are not included.
- The website is a simulation, not a backend-connected dashboard.

## What I Learned

- How to structure a master/worker system over TCP sockets
- How heartbeat messages can support basic failure detection
- How scheduling changes when workers can be idle, busy, or dead
- How to collect remote execution output and expose it through a client shell
- Why production distributed systems need stronger protocols, retries, and observability

## Future Improvements

- Split one large job into smaller task chunks
- Add dynamic worker registration instead of fixed ports
- Store job history in a small database
- Add a browser dashboard for live worker and job status
- Replace the plain-text protocol with structured messages
- Add automated integration tests for master/worker behavior

## More documentation

- [Architecture notes](docs/architecture.md)
- [Local demo guide](docs/demo.md)
- [Project scope](docs/portfolio-copy.md)
