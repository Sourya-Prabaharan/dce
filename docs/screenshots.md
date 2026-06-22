# Screenshot Checklist

Use clean terminal windows with a readable font size. Capture the actual logs from the project instead of mocked output.

## GitHub README

1. Master startup
   - Command: `python3 -m master.master`
   - Capture lines showing the master starting on port `9000`.

2. Workers connecting
   - Commands:
     - `python3 -m worker.worker1`
     - `python3 -m worker.worker2`
     - `python3 -m worker.worker3`
   - Capture the master showing worker heartbeat or connection logs.

3. Job assignment
   - Command: `./demo.sh`
   - Capture `[MASTER] Job ... received` and `[SCHEDULER] Assigned task ...`.

4. Result collection
   - Capture `[RESULT] ... completed task ...` and `[FINAL] Distributed job ... completed`.

## LinkedIn

Use one image that shows:

- master terminal on the left
- worker terminals on the right
- a completed job result visible

Recommended caption:

```text
Built a small distributed compute engine with a TCP master/worker architecture, heartbeat-based worker tracking, and Docker-backed job execution.
```

## Portfolio Page

Use three screenshots:

1. Architecture diagram from the README
2. Master + worker terminals during a live run
3. Final result aggregation output

## Optional GIF

Record a 20-30 second clip:

1. Start master
2. Start two or three workers
3. Submit `examples/distributed_sum.py`
4. Show final output
5. Stop one worker and show failure detection

Placeholder path:

```text
docs/demo/dce-demo.gif
```
