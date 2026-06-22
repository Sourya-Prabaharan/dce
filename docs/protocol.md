# Protocol Notes

The project uses a small plain-text TCP protocol. It is meant to be easy to inspect in terminal logs while learning the system.

## Commands

### Submit A Job

```text
SUBMIT <job_id> <code_length>
<python source code>
```

The master stores the job, assigns it to an idle worker if one is available, or keeps it in the FIFO queue.

### Run A Job

```text
RUN <job_id> <code_length>
<python source code>
```

The master sends this message to a worker.

### Complete A Job

```text
DONE <job_id> <output_length>
<program output>
```

The worker sends this message back to the master after running the job.

### Heartbeat

```text
HEARTBEAT <worker_id>
```

Workers send this periodically so the master knows they are still alive.

### Status

```text
STATUS <job_id>
```

Returns the current job status.

### Logs

```text
LOGS <job_id>
```

Returns saved output for a completed job.

## Current Tradeoffs

- Messages are plain text for readability.
- There is no authentication or encryption.
- There is no durable message queue.
- Larger projects would likely use structured messages such as JSON, Protobuf, or another RPC format.
