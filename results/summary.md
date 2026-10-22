| Workload | Workers | Mean wall time (s) | Min–max (s) | Jobs/s | Speedup vs 1 worker | Slot utilization |
|---|---:|---:|---:|---:|---:|---:|
| primes | 1 | 0.7464 | 0.6890–0.8955 | 1.34 | 1.00x | 98.5% |
| primes | 2 | 0.4278 | 0.3687–0.6452 | 2.34 | 1.74x | 96.3% |
| primes | 3 | 0.2798 | 0.2602–0.3143 | 3.57 | 2.67x | 93.8% |
| csv | 1 | 0.6666 | 0.6522–0.6865 | 1.50 | 1.00x | 98.8% |
| csv | 2 | 0.5392 | 0.5113–0.5784 | 1.85 | 1.24x | 99.0% |
| csv | 3 | 0.4538 | 0.4377–0.4818 | 2.20 | 1.47x | 98.1% |

| Failure mode | Trial | Observed reassignment (ms) | Correct recovery | Within 5 s |
|---|---:|---:|---|---|
| SIGKILL | 1 | 376.865 | yes | yes |
| SIGKILL | 2 | 381.787 | yes | yes |
| SIGKILL | 3 | 402.660 | yes | yes |
| SIGKILL | 4 | 348.263 | yes | yes |
| SIGKILL | 5 | 363.007 | yes | yes |
| SIGSTOP | 1 | 1995.870 | yes | yes |
| SIGSTOP | 2 | 2005.960 | yes | yes |
| SIGSTOP | 3 | 2062.730 | yes | yes |
