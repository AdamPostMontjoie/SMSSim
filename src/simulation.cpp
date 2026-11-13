#include "simulation.h"
#include "message_queue.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <condition_variable>

namespace {
MessageQueue *mq = nullptr;
int message_count = 0;
int sender_count = 0;
unsigned long simulation_seed = 0;
double mean_send_wait_ms = 0;
double progress_interval_seconds = 0;
bool progress_display_started = false;

//Cv for the progress monitor
condition_variable monitor_cv;
// Read or update these statistics while holding stats_mutex.
mutex stats_mutex;
int messages_sent = 0;
int messages_failed = 0;
double total_send_time_ms = 0;  // Includes successful and failed sends.
Message last_sent_message{-1, ""};
bool last_message_failed = false;
chrono::steady_clock::time_point simulation_start;

void print_stats(int sent, int failed, double send_time_ms, double elapsed_seconds, const Message &last_sent, bool last_failed, bool progressive_print) {
  int processed = sent + failed;
  double average_send_time_ms = processed > 0 ? send_time_ms / processed : 0;
  double throughput = elapsed_seconds > 0 ? processed / elapsed_seconds : 0;
  int count_width = max(5, static_cast<int>(to_string(message_count).size()));
  ostringstream line;
  line << setprecision(4)
       << "Sent: " << setw(count_width) << sent
       << " Failed: " << setw(count_width) << failed
       << " Avg ms: " << setw(10) << average_send_time_ms
       << " Messages/s: " << setw(10) << throughput;
  ostringstream message_line;
  if (last_sent.id >= 0) {
    message_line << "Last sent message | Number: " << last_sent.id;
    if (last_failed) {
      message_line << " | message failed";
    } else {
      message_line << " | Text: " << last_sent.text;
    }
  } else {
    message_line << "Last sent message: none";
  }
  string preview = message_line.str();
  if (preview.size() > 79) {
    preview.resize(76);
    preview += "...";
  }
  if (!progressive_print) {
    if (progress_display_started) {
      cout << "\r\033[1A";
    }
    cout << "\r\033[2K";
  }
  cout << line.str() << '\n';
  if (!progressive_print) {
    cout << "\r\033[2K";
  }
  cout << preview;
  if (progressive_print) {
    cout << '\n';
  }
  cout << flush;
  progress_display_started = true;
}
}

void InitSimulation(int messages, int num_senders, double mean_send_wait, double mean_failure_rate, unsigned long seed, double progress_interval, bool progressive_print) {

  // Shared message queue
  mq = new MessageQueue(2 * num_senders);
  message_count = messages;
  sender_count = num_senders;
  simulation_seed = seed;
  mean_send_wait_ms = mean_send_wait;
  progress_interval_seconds = progress_interval;
  progress_display_started = false;

  // Reproducible RNG
  mt19937 rng(seed);
  uniform_real_distribution<double> unit(-1.0, 1.0);
  vector<double> failure_rates(num_senders, mean_failure_rate);

  // Fill each sender's failure rate.
  for (int i = 0; i + 1 < num_senders; i += 2) {
    double offset = 0.5 * min(mean_failure_rate, 1.0 - mean_failure_rate) * unit(rng);
    failure_rates[i] = mean_failure_rate + offset;
    failure_rates[i + 1] = mean_failure_rate - offset;
  }

  // An unpaired value keeps the mean; shuffle the assignments reproducibly.
  shuffle(failure_rates.begin(), failure_rates.end(), rng);
  {
    lock_guard<mutex> lock(stats_mutex);
    messages_sent = 0;
    messages_failed = 0;
    total_send_time_ms = 0;
    last_sent_message = {-1, ""};
    last_message_failed = false;
    simulation_start = chrono::steady_clock::now();
  }

  vector<std::thread> senders;
  senders.reserve(num_senders);
  //Start producer thread and sender threads
  thread prod(producer);
  for (int i = 0; i < num_senders; i++){
    senders.emplace_back(sender, failure_rates[i], i);
  }
  //set progress intervals
  auto interval = chrono::duration<double>(progress_interval_seconds);
  auto next_update = chrono::steady_clock::now() + interval;
  //progress monitor loop
  while (true) {
    // Main thread waits
    this_thread::sleep_until(next_update);

    unique_lock<mutex> lock(stats_mutex);

    bool finished = messages_sent + messages_failed == message_count;
    int sent_snapshot = messages_sent;
    int failed_snapshot = messages_failed;
    double send_time_snapshot = total_send_time_ms;
    Message last_sent_snapshot = last_sent_message;
    bool last_failed_snapshot = last_message_failed;
    double elapsed_seconds = chrono::duration<double>(chrono::steady_clock::now() - simulation_start).count();

    lock.unlock(); 

    if (finished) {
        break;
    }

    //Print progress monitor
    print_stats(sent_snapshot, failed_snapshot, send_time_snapshot, elapsed_seconds,
                last_sent_snapshot, last_failed_snapshot, progressive_print);

    next_update += interval;

    // If printing fell behind, schedule a future refresh.
    auto now = chrono::steady_clock::now();
    if (next_update <= now) {
        next_update = now + interval;
    }
}

  prod.join();
  for (int i = 0; i < num_senders; i++){
    senders[i].join();
  }

  {
    lock_guard<mutex> lock(stats_mutex);
    double elapsed_seconds = chrono::duration<double>(
        chrono::steady_clock::now() - simulation_start).count();

    print_stats(messages_sent, messages_failed, total_send_time_ms, elapsed_seconds,
                last_sent_message, last_message_failed, progressive_print);
  }
  if (!progressive_print) {
    cout << endl;
  }

  delete mq;
  mq = nullptr;
}

void producer() {
  mt19937 rng(simulation_seed);
  uniform_int_distribution<int> length_dist(1, 100);
  uniform_int_distribution<int> character_dist(32, 126);

  for (int id = 0; id < message_count; id++) {
    Message message;
    message.id = id;
    int length = length_dist(rng);

    for (int i = 0; i < length; i++) {
      message.text += static_cast<char>(character_dist(rng));
    }
    mq->append(message);
  }

  // Stop markers follow all real messages, one for each sender.
  for (int i = 0; i < sender_count; i++) {
    mq->append({-1, ""});
  }
}

void sender(double failure_rate, int sender_id) {
  seed_seq wait_seed{simulation_seed, 0UL, static_cast<unsigned long>(sender_id)};
  mt19937 wait_rng(wait_seed);
  seed_seq failure_seed{simulation_seed, 1UL, static_cast<unsigned long>(sender_id)};
  mt19937 failure_rng(failure_seed);
  uniform_real_distribution<double> wait_offset(-1.0, 1.0);
  bernoulli_distribution failure(failure_rate);

  while (true) {
    Message message = mq->remove();
    if (message.id == -1) {
      return;
    }

    double send_wait_ms = mean_send_wait_ms * (1.0 + 0.5 * wait_offset(wait_rng));
    auto send_start = chrono::steady_clock::now();
    this_thread::sleep_for(chrono::duration<double, milli>(send_wait_ms));
    bool failed = failure(failure_rng);
    double send_time_ms = chrono::duration<double, milli>(chrono::steady_clock::now() - send_start).count();
    //lock to update stats
    {
      lock_guard<mutex> lock(stats_mutex);
      if (failed) {
        messages_failed++;
      } else {
        messages_sent++;
      }
      last_sent_message = message;
      last_message_failed = failed;
      total_send_time_ms += send_time_ms;
    }
  }
}
