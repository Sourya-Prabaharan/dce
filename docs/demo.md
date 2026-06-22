# Demo Guide

Use this guide to record a short project demo or capture screenshots for GitHub and LinkedIn.

## 1. Start The Demo Helper

```bash
./demo.sh
```

The script checks the Python files and starts the master process.

## 2. Start Workers

Open three separate terminals from the project root.

Terminal 1:

```bash
python3 -m worker.worker1
```

Terminal 2:

```bash
python3 -m worker.worker2
```

Terminal 3:

```bash
python3 -m worker.worker3
```

After the workers start, the master terminal should show heartbeat logs.

## 3. Submit Sample Jobs

Return to the `demo.sh` terminal and press Enter. The script submits the sample workload a few times so multiple workers can receive jobs.

Expected log examples:

```text
[MASTER] Job demo1 received
[SCHEDULER] Assigned task demo1 to worker1
[RESULT] worker1 completed task demo1
[FINAL] Distributed job demo1 completed in 1200 ms
```

## 4. Show Failure Detection

While the master is still running, stop one worker with `Ctrl+C`. Wait for the heartbeat timeout. The master should mark the worker dead:

```text
[MASTER] Worker worker2 missed heartbeat - marking dead
```

## 5. End The Demo

Stop the demo helper with `Ctrl+C`. Then stop any remaining worker terminals.

## Notes

- The numbered workers use Docker to run submitted Python jobs.
- If Docker is not running, start Docker Desktop or the Docker daemon before the demo.
- The project currently assigns whole submitted jobs to workers. It does not split one job into smaller chunks yet.
