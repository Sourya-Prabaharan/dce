# Portfolio Copy

## Short Description

A small distributed compute engine that uses a TCP master/worker architecture to submit jobs, assign work to available workers, track worker health with heartbeats, and collect job output.

## Longer Project Description

This project is a learning-focused distributed systems project built around a master node and several worker processes. The master accepts job submissions over TCP, keeps a FIFO queue of waiting jobs, assigns work to idle workers, monitors worker heartbeats, and stores results when workers finish. The worker processes execute submitted Python scripts inside Docker containers and send the output back to the master.

The project is intentionally small, but it demonstrates practical systems concepts: socket programming, concurrency with threads, worker health tracking, basic scheduling, and failure handling.

## Resume / Portfolio Bullets

- Built a Python TCP master/worker system that assigns submitted jobs to available workers and stores job output for later inspection.
- Added heartbeat-based worker monitoring so the master can detect unavailable workers and requeue running jobs.
- Created a local demo workflow with Docker-backed workers, sample jobs, and clear terminal logs for debugging and presentation.

## Tech Stack

Python, TCP sockets, threads, Docker, Bash

## Screenshots To Use

- Master starting and waiting for workers
- Three workers sending heartbeats
- Job assignment from scheduler to worker
- Final result output after a worker completes a job
- Worker failure detection after stopping one worker

## Suggested LinkedIn Description

I built a small distributed compute engine to practice systems programming concepts: TCP sockets, master/worker scheduling, worker heartbeats, and result collection. It is not production-scale, but it helped me understand how distributed job runners track workers, assign work, and recover when a worker stops responding.

## Suggested GitHub Description

Small Python distributed compute engine with a TCP master/worker architecture, heartbeat-based worker tracking, Docker-backed job execution, and a local demo workflow.
