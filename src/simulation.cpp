#include "simulation.h"
#include "message_queue.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <vector>

void InitSimulation(int messages, int num_senders, double mean_send_wait,
                    double mean_failure_rate, unsigned long seed,
                    double progress_interval) {
  // Shared message queue
  MessageQueue *mq = new MessageQueue(2 * num_senders);
  // Reproducible RNG

  mt19937 rng(seed);
  uniform_real_distribution<double> unit(-1.0, 1.0);
  vector<double> send_waits(num_senders, mean_send_wait);
  vector<double> failure_rates(num_senders, mean_failure_rate);

  // Fill each sender's mean send wait.
  for (int i = 0; i + 1 < num_senders; i += 2) {
    double offset = 0.5 * mean_send_wait * unit(rng);
    send_waits[i] = mean_send_wait + offset;
    send_waits[i + 1] = mean_send_wait - offset;
  }

  // Fill each sender's failure rate.
  for (int i = 0; i + 1 < num_senders; i += 2) {
    double offset = 0.5 * min(mean_failure_rate, 1.0 - mean_failure_rate) * unit(rng);
    failure_rates[i] = mean_failure_rate + offset;
    failure_rates[i + 1] = mean_failure_rate - offset;
  }

  // An unpaired sender keeps the mean; shuffle to randomize sender assignments.
  shuffle(send_waits.begin(), send_waits.end(), rng);
  shuffle(failure_rates.begin(), failure_rates.end(), rng);

  // TODO: Start one producer thread and the requested number of sender threads.
  thread prod(producer);
  vector<std::thread> senders;

  senders.reserve(num_senders);
  for (int i = 0; i < num_senders; i++){
    senders.emplace_back(sender, send_waits[i], failure_rates[i], i);
  }

  //
  prod.join();
  for (int i = 0; i < num_senders; i++){
    senders[i].join();
  }
  // TODO: Join all threads before returning.
}

void producer() {
  // TODO: Generate the requested number of messages, each with up to 100 random characters.
  // TODO: Append each message to the shared queue.
  // TODO: Signal the senders when production is complete.
}

void sender(double send_wait,  double failure_rate) {
  // TODO: Consume messages from the shared queue until production is complete and it is empty.
  // TODO: Simulate random send waits using mean_wait_ms and failures using failure_rate.
  // TODO: Use this sender's random stream and update the shared progress statistics.
}
