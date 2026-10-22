Generate numeric input with `build/dce-cli generate --output data/numbers.csv --rows 10000`.
CSV files have no headers: every comma-separated cell is a finite number. Blank rows,
empty cells, strings, NaN, infinity, and values above 1e100 in magnitude are invalid.
A trailing newline is accepted. Empty files produce count/sum zero and null statistics.
Workers partition by byte ranges, then align to complete rows. Each row belongs to
exactly one task. All cells contribute to the same population statistics.
Use an identical, immutable shared directory on coordinator and workers; in Compose
this directory is mounted read-only as /data. Do not modify inputs during a job.
