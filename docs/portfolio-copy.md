# Project scope

DCE is a Python learning project for TCP master/worker scheduling, a FIFO waiting queue, worker heartbeats, and collection of script output. Numbered workers execute whole Python scripts using Docker. The master marks workers dead after a heartbeat timeout and requeues their running jobs; it does not resume partial execution or guarantee exactly-once processing.

The static website is a JavaScript simulation. Kubernetes manifests, dynamic worker registration, durable state, and authenticated execution are not implemented. The code uses fixed localhost ports and is intended for trusted local demonstrations.
