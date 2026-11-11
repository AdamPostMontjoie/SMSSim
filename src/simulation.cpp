#include "simulation.h"
#include "message_queue.h"
#include <chrono>
#include <random>
#include <thread>
#include <vector>

void InitSimulation(int messages, int senders, double mean_send_wait,
                    double mean_failure_rate, unsigned long seed,
                    double progress_interval) {
  // TODO: Set up the shared queue and make these options available to the workers.
  
  // TODO: Use the seed to initialize independent producer and per-sender random streams.
  // TODO: Assign each sender its own failure rate using mean_failure_rate.
  // TODO: Start one producer thread and the requested number of sender threads.
  // TODO: Start the progress monitor using progresss_interval.
  // TODO: Join all threads before returning.
}

void *producer() {
  // TODO: Generate the requested number of messages, each with up to 100 random characters.
  // TODO: Append each message to the shared queue.
  // TODO: Signal the senders when production is complete.
}

void *sender(double send_wait,  double failure_rate) {
  // TODO: Consume messages from the shared queue until production is complete and it is empty.
  // TODO: Simulate random send waits using mean_wait_ms and failures using failure_rate.
  // TODO: Use this sender's random stream and update the shared progress statistics.
}
