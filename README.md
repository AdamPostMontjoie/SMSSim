# SMS Alert Simulation

A C++17 simulation of concurrent SMS delivery with configurable message generation, sender failure probabilities, reproducible randomness, and progress reporting.

Requires a C++17 compiler and `make`. The test runner uses POSIX APIs and supports macOS/Linux. No Python is required.

From the repository root:

```sh
make -C src
./src/sms-sim --messages 1000 --senders 4 --mean-wait-ms 100 \
  --mean-failure-rate 0.1 --seed 42 --progress-interval 1

make -C src tests
make -C src clean
```

Available options:

| Option | Default |
|---|---|
| `--messages` | 1000 |
| `--senders` | 4 |
| `--mean-wait-ms` | 100 ms |
| `--mean-failure-rate` | 0.1 |
| `--seed` | Automatically chosen and printed |
| `--progress-interval` | 1 second |
| `--progressive-print` | Off; enable to retain each update |

A producer generates sequentially numbered messages containing 1–100 printable characters. Messages pass through a mutex-protected circular queue with capacity twice the sender count. Condition variables stop production when the queue is full and block senders when it is empty. Stop markers terminate the senders after production finishes.

Each sender draws a fresh delay between 50% and 150% of the configured mean and independently simulates failures using its assigned probability. Sender probabilities vary while preserving the configured pool average.

Separate wait and failure generators are seeded using the main seed, sender number, and a purpose identifier. The same seed and settings reproduce messages and each sender’s random sequences within the same build. Scheduling, observed outcomes, and measured timings can vary.

The monitor displays successful sends, failures, average send duration including failures, and processed messages per second. A second line shows the latest completed message’s number and preview or failure status. Default mode refreshes both lines; `--progressive-print` appends snapshots for logging.

The tests cover seeded replay, input boundaries, queue backpressure, extra senders, failure-rate endpoints, and monitor intervals. A parser-only test executable checks integer limits without running an enormous simulation.

Completion is reported on the next monitor tick. Default refresh mode expects an ANSI-capable terminal approximately 80 columns wide.
