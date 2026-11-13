#ifdef CLI_PARSER_STUB
// Parse arguments without starting a simulation.
#include "simulation.h"
#include <iostream>

void InitSimulation(int messages, int num_senders, double, double, unsigned long,
                    double, bool) {
  std::cout << "Parsed message count: " << messages << '\n'
            << "Parsed sender count: " << num_senders << '\n';
}

#else
#include "simulation.h"
#ifndef RNG_TRACE_H
#define RNG_TRACE_H

#include <cstddef>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <vector>

// Record RNG draws without changing the generators' output.
namespace rng_test {

struct Trace {
  std::map<std::string, std::vector<std::mt19937::result_type>> draws;
  std::map<int, double> failure_rates;
};

namespace detail {
inline std::mutex capture_mutex;
inline Trace capture;
inline std::size_t scalar_count = 0;
inline thread_local int current_sender_id = -1;
}

inline void reset() {
  std::lock_guard<std::mutex> lock(detail::capture_mutex);
  detail::capture = Trace{};
  detail::scalar_count = 0;
  detail::current_sender_id = -1;
}

inline Trace snapshot() {
  std::lock_guard<std::mutex> lock(detail::capture_mutex);
  return detail::capture;
}

class Engine {
 public:
  using result_type = std::mt19937::result_type;

  explicit Engine(result_type seed) : engine_(seed) {
    std::lock_guard<std::mutex> lock(detail::capture_mutex);
    key_ = "scalar/" + std::to_string(detail::scalar_count++);
    detail::capture.draws.try_emplace(key_);
    detail::current_sender_id = -1;
  }

  explicit Engine(std::seed_seq &sequence) : engine_(sequence) {
    std::vector<std::seed_seq::result_type> values(sequence.size());
    sequence.param(values.begin());
    if (values.size() == 3 && (values[1] == 0 || values[1] == 1)) {
      key_ = (values[1] == 0 ? "wait/" : "failure/") +
             std::to_string(values[2]);
      detail::current_sender_id = static_cast<int>(values[2]);
    } else {
      key_ = "seed";
      for (auto value : values) {
        key_ += "/" + std::to_string(value);
      }
      detail::current_sender_id = -1;
    }
    std::lock_guard<std::mutex> lock(detail::capture_mutex);
    detail::capture.draws.try_emplace(key_);
  }

  static constexpr result_type min() { return std::mt19937::min(); }
  static constexpr result_type max() { return std::mt19937::max(); }

  result_type operator()() {
    result_type value = engine_();
    std::lock_guard<std::mutex> lock(detail::capture_mutex);
    detail::capture.draws[key_].push_back(value);
    return value;
  }

 private:
  std::mt19937 engine_;
  std::string key_;
};

class FailureDistribution {
 public:
  explicit FailureDistribution(double probability) : distribution_(probability) {
    std::lock_guard<std::mutex> lock(detail::capture_mutex);
    detail::capture.failure_rates[detail::current_sender_id] = probability;
  }

  template <typename Generator>
  bool operator()(Generator &generator) {
    return distribution_(generator);
  }

 private:
  std::bernoulli_distribution distribution_;
};

}

#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define mt19937 rng_test::Engine
#define bernoulli_distribution rng_test::FailureDistribution
#include "../simulation.cpp"
#undef bernoulli_distribution
#undef mt19937

namespace {
void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

class CaptureOutput {
 public:
  CaptureOutput() : original_(std::cout.rdbuf(output_.rdbuf())) {}
  ~CaptureOutput() { std::cout.rdbuf(original_); }

 private:
  std::ostringstream output_;
  std::streambuf *original_;
};

std::vector<Message> generate_messages(unsigned long seed, int count) {
  auto queue = std::make_unique<MessageQueue>(count + 1);
  mq = queue.get();
  message_count = count;
  sender_count = 0;
  simulation_seed = seed;
  producer();

  std::vector<Message> messages;
  for (int id = 0; id < count; id++) {
    Message message = mq->remove();
    require(message.id == id, "Message IDs must start at zero and be sequential");
    require(!message.text.empty() && message.text.size() <= 100,
            "Message length must be between 1 and 100");
    messages.push_back(message);
  }
  require(mq->isEmpty(), "Producer generated more messages than requested");
  mq = nullptr;
  return messages;
}

rng_test::Trace run_simulation(int count, int workers, double failure_rate) {
  rng_test::reset();
  CaptureOutput capture;
  InitSimulation(count, workers, 0, failure_rate, 42, 0.001, true);
  require(messages_sent + messages_failed == count, "Processed count is incorrect");
  require(mq == nullptr, "Simulation did not release its queue after joining");
  return rng_test::snapshot();
}

rng_test::Trace run_sender(unsigned long seed, int id, int first_message_id) {
  constexpr int count = 32;
  auto queue = std::make_unique<MessageQueue>(count + 1);
  mq = queue.get();
  simulation_seed = seed;
  mean_send_wait_ms = 0;
  messages_sent = messages_failed = 0;
  total_send_time_ms = 0;
  last_sent_message = {-1, ""};
  last_message_failed = false;
  for (int i = 0; i < count; i++) {
    mq->append({first_message_id + i, "payload"});
  }
  mq->append({-1, ""});

  rng_test::reset();
  sender(0.35, id);
  require(messages_sent + messages_failed == count, "Sender did not process its batch");
  require(mq->isEmpty(), "Sender did not consume its stop marker");
  mq = nullptr;
  return rng_test::snapshot();
}

// The same seed reproduces messages, failure rates, and sender RNG streams.
void seeded_replay() {
  const auto first_messages = generate_messages(42, 48);
  const auto repeated_messages = generate_messages(42, 48);
  const auto other_messages = generate_messages(43, 48);
  bool changed = false;
  for (std::size_t i = 0; i < first_messages.size(); i++) {
    require(first_messages[i].text == repeated_messages[i].text,
            "The same seed changed message text");
    changed |= first_messages[i].text != other_messages[i].text;
  }
  require(changed, "These different seeds unexpectedly generated identical batches");

  const auto first = run_simulation(64, 3, 0.3);
  const auto repeated = run_simulation(64, 3, 0.3);
  require(first.failure_rates == repeated.failure_rates,
          "The same seed changed a sender's failure probability");
  double probability_sum = 0;
  for (const auto &entry : first.failure_rates) {
    require(entry.second >= 0 && entry.second <= 1, "Invalid sender probability");
    probability_sum += entry.second;
  }
  require(first.failure_rates.size() == 3 && std::abs(probability_sum / 3 - 0.3) < 1e-12,
          "Configured sender probabilities do not average to the requested mean");

  // Scheduling changes stream lengths; compare the shared prefix.
  for (const auto &entry : first.draws) {
    const auto found = repeated.draws.find(entry.first);
    require(found != repeated.draws.end(), "An RNG stream disappeared on replay");
    const auto length = std::min(entry.second.size(), found->second.size());
    require(std::equal(entry.second.begin(), entry.second.begin() + length,
                       found->second.begin()), "An RNG draw prefix changed on replay");
  }

  // Give each sender work so idle threads cannot skip the replay check.
  for (int id = 0; id < 3; id++) {
    const auto one = run_sender(42, id, 0);
    const auto two = run_sender(42, id, 1000);
    const auto wait_key = "wait/" + std::to_string(id);
    const auto failure_key = "failure/" + std::to_string(id);
    require(!one.draws.at(wait_key).empty() && !one.draws.at(failure_key).empty(),
            "A sender did not use both RNG streams");
    require(one.draws == two.draws, "Sender draws depended on message IDs");
    require(one.draws.at(wait_key) != one.draws.at(failure_key),
            "Wait and failure RNG streams unexpectedly matched");
    const auto other_seed = run_sender(43, id, 0);
    require(one.draws.at(wait_key) != other_seed.draws.at(wait_key),
            "Changing the seed did not change this sender's wait stream");
    require(one.draws.at(failure_key) != other_seed.draws.at(failure_key),
            "Changing the seed did not change this sender's failure stream");
  }
}

// Idle senders exit when there are fewer messages than senders.
void extra_senders() {
  const auto trace = run_simulation(3, 12, 0);
  require(messages_sent == 3 && messages_failed == 0,
          "Extra senders changed the processed totals");
  require(trace.failure_rates.size() == 12,
          "The configured sender pool was silently reduced");
}

// A full queue blocks the producer and preserves FIFO order when it resumes.
void backed_up_queue() {
  using namespace std::chrono_literals;
  {
    MessageQueue queue(1);
    queue.append({0, "first"});
    std::promise<void> entered;
    auto started = entered.get_future();
    auto writer = std::async(std::launch::async, [&] {
      entered.set_value();
      queue.append({1, "second"});
    });
    started.wait();
    bool blocked = writer.wait_for(30ms) == std::future_status::timeout;
    const auto first = queue.remove();
    writer.get();
    const auto second = queue.remove();
    require(blocked, "A full queue must block its producer until space is available");
    require(first.id == 0 && second.id == 1, "Backpressure changed FIFO order");
    require(queue.isEmpty(), "Queue should be empty after removing both messages");
  }

  MessageQueue queue(2);
  constexpr int count = 2000;
  std::thread writer([&] {
    for (int id = 0; id < count; id++) queue.append({id, "payload"});
  });
  bool ordered = true;
  for (int id = 0; id < count; id++) {
    const auto message = queue.remove();
    ordered &= message.id == id && message.text == "payload";
  }
  writer.join();
  require(ordered && queue.isEmpty(), "Queue lost, duplicated, or reordered messages");
}
}

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <signal.h>

namespace test_runner {
std::string application;
std::string unit_tests;
std::string parser_tests;

struct ProcessResult {
  int returncode;
  std::string stdout_text;
  std::string stderr_text;
};

void check(bool condition, const std::string &message) {
  if (!condition) throw std::runtime_error(message);
}

void close_descriptor(int &descriptor) {
  if (descriptor >= 0) {
    close(descriptor);
    descriptor = -1;
  }
}

void drain_descriptor(int &descriptor, std::string &output) {
  char buffer[4096];
  for (int reads = 0; descriptor >= 0 && reads < 16; reads++) {
    const ssize_t count = read(descriptor, buffer, sizeof(buffer));
    if (count > 0) {
      output.append(buffer, static_cast<std::size_t>(count));
    } else if (count == 0) {
      close_descriptor(descriptor);
    } else if (errno == EINTR) {
      continue;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    } else {
      throw std::runtime_error(std::string("Reading child output failed: ") +
                               std::strerror(errno));
    }
  }
}

ProcessResult execute(const std::string &path, const std::vector<std::string> &args) {
  int out_pipe[2] = {-1, -1};
  int err_pipe[2] = {-1, -1};
  if (pipe(out_pipe) < 0) {
    throw std::runtime_error(std::string("Creating stdout pipe failed: ") +
                             std::strerror(errno));
  }
  if (pipe(err_pipe) < 0) {
    const int error = errno;
    close_descriptor(out_pipe[0]);
    close_descriptor(out_pipe[1]);
    throw std::runtime_error(std::string("Creating stderr pipe failed: ") +
                             std::strerror(error));
  }
  for (int descriptor : {out_pipe[0], err_pipe[0]}) {
    const int flags = fcntl(descriptor, F_GETFL, 0);
    if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) < 0) {
      const int error = errno;
      close_descriptor(out_pipe[0]);
      close_descriptor(out_pipe[1]);
      close_descriptor(err_pipe[0]);
      close_descriptor(err_pipe[1]);
      throw std::runtime_error(std::string("Configuring child pipes failed: ") +
                               std::strerror(error));
    }
  }

  std::vector<std::string> words{path};
  words.insert(words.end(), args.begin(), args.end());
  std::vector<char *> arguments;
  for (auto &word : words) arguments.push_back(const_cast<char *>(word.c_str()));
  arguments.push_back(nullptr);

  const pid_t child = fork();
  if (child < 0) {
    const int error = errno;
    close_descriptor(out_pipe[0]);
    close_descriptor(out_pipe[1]);
    close_descriptor(err_pipe[0]);
    close_descriptor(err_pipe[1]);
    throw std::runtime_error(std::string("Starting child failed: ") +
                             std::strerror(error));
  }
  if (child == 0) {
    setpgid(0, 0);
    const rlimit no_core{0, 0};
    setrlimit(RLIMIT_CORE, &no_core);
    if (dup2(out_pipe[1], STDOUT_FILENO) < 0 ||
        dup2(err_pipe[1], STDERR_FILENO) < 0) {
      _exit(126);
    }
    for (int descriptor : {out_pipe[0], out_pipe[1], err_pipe[0], err_pipe[1]}) {
      if (descriptor > STDERR_FILENO) close(descriptor);
    }
    execv(path.c_str(), arguments.data());
    const char message[] = "Unable to execute test child\n";
    const ssize_t ignored = write(STDERR_FILENO, message, sizeof(message) - 1);
    (void)ignored;
    _exit(127);
  }

  setpgid(child, child);
  close_descriptor(out_pipe[1]);
  close_descriptor(err_pipe[1]);
  ProcessResult result{0, {}, {}};
  int status = 0;
  bool reaped = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  try {
    while (!reaped || out_pipe[0] >= 0 || err_pipe[0] >= 0) {
      if (!reaped) {
        const pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) reaped = true;
        else if (waited < 0 && errno != EINTR) {
          throw std::runtime_error(std::string("Waiting for child failed: ") +
                                   std::strerror(errno));
        }
      }
      drain_descriptor(out_pipe[0], result.stdout_text);
      drain_descriptor(err_pipe[0], result.stderr_text);
      if (reaped && out_pipe[0] < 0 && err_pipe[0] < 0) break;
      const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline - std::chrono::steady_clock::now()).count();
      if (remaining <= 0) throw std::runtime_error("Timed out after 10 seconds: " + path);
      pollfd descriptors[2] = {
          {out_pipe[0], POLLIN | POLLHUP | POLLERR, 0},
          {err_pipe[0], POLLIN | POLLHUP | POLLERR, 0}};
      const int polled = poll(descriptors, 2, static_cast<int>(std::min<long long>(50, remaining)));
      if (polled < 0 && errno != EINTR) {
        throw std::runtime_error(std::string("Polling child output failed: ") +
                                 std::strerror(errno));
      }
    }
  } catch (...) {
    kill(-child, SIGKILL);
    if (!reaped) {
      kill(child, SIGKILL);
      while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    }
    close_descriptor(out_pipe[0]);
    close_descriptor(err_pipe[0]);
    throw;
  }
  result.returncode = WIFEXITED(status) ? WEXITSTATUS(status) :
                      WIFSIGNALED(status) ? -WTERMSIG(status) : -1;
  return result;
}

struct SubcaseFailures {
  std::vector<std::string> errors;
  template <typename Function>
  void run(const std::string &label, Function function) {
    try {
      function();
    } catch (const std::exception &error) {
      errors.push_back(label + ": " + error.what());
    }
  }
  void finish() const {
    if (errors.empty()) return;
    std::ostringstream message;
    for (std::size_t i = 0; i < errors.size(); i++) {
      if (i) message << '\n';
      message << errors[i];
    }
    throw std::runtime_error(message.str());
  }
};

ProcessResult run_cli(const std::string &path, const std::vector<std::string> &args) {
  std::vector<std::string> arguments{"--seed", "42", "--progressive-print"};
  arguments.insert(arguments.end(), args.begin(), args.end());
  return execute(path, arguments);
}

ProcessResult run_application(const std::vector<std::string> &args) {
  return run_cli(application, args);
}

long long integer_limit() {
  const auto result = execute(unit_tests, {"int-limit"});
  check(result.returncode == 0, "int-limit helper failed: " + result.stderr_text);
  std::istringstream input(result.stdout_text);
  long long limit;
  check(static_cast<bool>(input >> limit), "Invalid int-limit helper output");
  input >> std::ws;
  check(input.eof(), "Unexpected text in int-limit helper output");
  return limit;
}

void graceful_rejection(const ProcessResult &result) {
  check(result.returncode > 0,
        "Expected normal nonzero exit, got " + std::to_string(result.returncode) +
        ": " + result.stderr_text);
  check(result.stderr_text.find_first_not_of(" \t\r\n\f\v") != std::string::npos,
        "Rejection must explain the error on stderr");
  check(result.stdout_text.find("Parsed message count:") == std::string::npos,
        "Rejected input reached the simulation stub");
}

struct FinalStats {
  int sent;
  int failed;
  double average_ms;
  double throughput;
};

FinalStats final_stats(const ProcessResult &result) {
  check(result.returncode == 0, "Application failed: " + result.stderr_text);
  std::istringstream output(result.stdout_text);
  std::string line;
  std::string last_stats;
  while (std::getline(output, line)) {
    if (line.rfind("Sent:", 0) == 0) last_stats = line;
  }
  check(!last_stats.empty(), "No Sent: statistics row was printed");
  std::istringstream fields(last_stats);
  std::vector<std::string> values;
  while (fields >> line) values.push_back(line);
  auto integer = [](const std::string &value) {
    std::size_t consumed;
    int parsed = std::stoi(value, &consumed);
    check(consumed == value.size(), "Invalid integer in statistics row");
    return parsed;
  };
  auto real = [](const std::string &value) {
    std::size_t consumed;
    double parsed = std::stod(value, &consumed);
    check(consumed == value.size(), "Invalid number in statistics row");
    return parsed;
  };
  return {integer(values.at(1)), integer(values.at(3)),
          real(values.at(6)), real(values.at(8))};
}

// Counts up to INT_MAX reach the simulation unchanged.
void message_counts_at_int_limit_parse_without_simulating() {
  const long long maximum = integer_limit();
  SubcaseFailures failures;
  for (long long count : {maximum - 1, maximum}) {
    failures.run(std::to_string(count), [&] {
      const auto result = execute(parser_tests,
          {"--messages", std::to_string(count), "--seed", "42"});
      check(result.returncode == 0, "Parser rejected an int-sized count: " + result.stderr_text);
      check(result.stdout_text.find("Parsed message count: " + std::to_string(count) + "\n") !=
                std::string::npos, "Parser did not forward the message count unchanged");
    });
  }
  failures.finish();
}

// Oversized message counts return an error instead of aborting.
void oversized_message_counts_are_rejected_gracefully() {
  const long long maximum = integer_limit();
  SubcaseFailures failures;
  for (const std::string &count : {
      std::to_string(static_cast<unsigned long long>(maximum) + 1ULL), std::string(80, '9')}) {
    failures.run(count, [&] {
      graceful_rejection(execute(parser_tests, {"--messages", count, "--seed", "42"}));
    });
  }
  failures.finish();
}

// Rates 0 and 1 produce all successes and all failures.
void failure_rate_endpoints() {
  SubcaseFailures failures;
  for (int probability : {0, 1}) {
    failures.run(std::to_string(probability), [&] {
      const auto stats = final_stats(run_application({
          "--messages", "12", "--senders", "3", "--mean-wait-ms", "0.1",
          "--mean-failure-rate", std::to_string(probability),
          "--progress-interval", "0.001"}));
      check(stats.sent == (probability == 0 ? 12 : 0) &&
            stats.failed == (probability == 0 ? 0 : 12), "Incorrect endpoint success/failure totals");
      check(stats.average_ms > 0, "Endpoint average send time must include all messages");
      check(stats.throughput > 0, "Endpoint throughput must include all messages");
    });
  }
  failures.finish();
}

// Failure rates outside [0, 1] are rejected.
void failure_rate_outside_bounds_is_rejected() {
  SubcaseFailures failures;
  for (const std::string &probability : {std::string("-0.01"), std::string("1.01")}) {
    failures.run(probability, [&] {
      graceful_rejection(run_application({"--messages", "1", "--mean-failure-rate", probability}));
    });
  }
  failures.finish();
}

// Zero messages reports zero stats and no last message.
void zero_messages() {
  const auto result = run_application({"--messages", "0", "--progress-interval", "0.001"});
  const auto stats = final_stats(result);
  check(stats.sent == 0 && stats.failed == 0, "Zero messages must have zero outcome totals");
  check(stats.average_ms == 0 && stats.throughput == 0, "Zero messages must have zero metrics");
  check(result.stdout_text.find("Last sent message: none") != std::string::npos,
        "Zero messages must show no last message");
}

// Zero wait still processes every message.
void zero_wait() {
  const auto stats = final_stats(run_application({
      "--messages", "20", "--mean-wait-ms", "0", "--mean-failure-rate", "0",
      "--progress-interval", "0.001"}));
  check(stats.sent == 20 && stats.failed == 0, "Zero-wait totals are incorrect");
  check(stats.average_ms >= 0, "Zero-wait measured average must be nonnegative");
  check(stats.throughput > 0, "Zero-wait throughput must be positive");
}

// Zero senders is rejected.
void zero_senders_is_rejected() {
  const auto result = run_application({"--messages", "1", "--senders", "0"});
  graceful_rejection(result);
  std::string error = result.stderr_text;
  std::transform(error.begin(), error.end(), error.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  check(error.find("senders") != std::string::npos,
        "Zero-sender error must mention senders");
}

// Positive update intervals do not change the processed total.
void low_and_high_positive_progress_intervals() {
  SubcaseFailures failures;
  for (const std::string &interval : {std::string("0.0005"), std::string("0.05")}) {
    failures.run(interval, [&] {
      const auto stats = final_stats(run_application({
          "--messages", "16", "--senders", "2", "--mean-wait-ms", "0.1",
          "--mean-failure-rate", "0", "--progress-interval", interval}));
      check(stats.sent == 16 && stats.failed == 0, "Progress interval changed the totals");
    });
  }
  failures.finish();
}

// Zero and negative update intervals are rejected.
void nonpositive_progress_intervals_are_rejected() {
  SubcaseFailures failures;
  for (const std::string &interval : {std::string("0"), std::string("-0.1")}) {
    failures.run(interval, [&] {
      graceful_rejection(run_application({
          "--messages", "1", "--progress-interval", interval}));
    });
  }
  failures.finish();
}
}  // namespace test_runner
#include <filesystem>
#include <functional>
#include <utility>

namespace {
void run_unit_case(const std::string &name) {
  const auto result = test_runner::execute(test_runner::unit_tests, {name});
  test_runner::check(result.returncode == 0, result.stdout_text + result.stderr_text);
}

int run_all_tests(const char *executable) {
  const auto directory = std::filesystem::absolute(executable).parent_path();
  test_runner::application = (directory / "sms-sim").string();
  test_runner::unit_tests = (directory / "simulation-tests").string();
  test_runner::parser_tests = (directory / "sms-sim-parser-tests").string();

  const std::vector<std::pair<std::string, std::function<void()>>> tests{
      {"test_backed_up_queue_blocks_resumes_and_preserves_messages",
       [] { run_unit_case("backed-up-queue"); }},
      {"test_failure_rate_endpoints", test_runner::failure_rate_endpoints},
      {"test_failure_rate_outside_bounds_is_rejected",
       test_runner::failure_rate_outside_bounds_is_rejected},
      {"test_low_and_high_positive_progress_intervals",
       test_runner::low_and_high_positive_progress_intervals},
      {"test_message_counts_at_int_limit_parse_without_simulating",
       test_runner::message_counts_at_int_limit_parse_without_simulating},
      {"test_more_senders_than_messages_finish_cleanly",
       [] { run_unit_case("extra-senders"); }},
      {"test_nonpositive_progress_intervals_are_rejected",
       test_runner::nonpositive_progress_intervals_are_rejected},
      {"test_oversized_message_counts_are_rejected_gracefully",
       test_runner::oversized_message_counts_are_rejected_gracefully},
      {"test_same_inputs_and_seed_replay", [] { run_unit_case("seeded-replay"); }},
      {"test_zero_messages", test_runner::zero_messages},
      {"test_zero_senders_is_rejected", test_runner::zero_senders_is_rejected},
      {"test_zero_wait", test_runner::zero_wait},
  };

  int failures = 0;
  for (const auto &test : tests) {
    try {
      test.second();
      std::cout << "PASS " << test.first << '\n';
    } catch (const std::exception &error) {
      failures++;
      std::cerr << "FAIL " << test.first << ": " << error.what() << '\n';
    }
  }
  std::cout << tests.size() << " tests, " << failures << " failures\n";
  return failures == 0 ? 0 : 1;
}
}

int main(int argc, char *argv[]) {
  if (argc == 1) {
    return run_all_tests(argv[0]);
  }
  if (argc != 2) {
    std::cerr << "Provide a test case name\n";
    return 1;
  }
  const std::string name = argv[1];
  if (name == "int-limit") {
    std::cout << std::numeric_limits<int>::max() << '\n';
    return 0;
  }
  try {
    if (name == "seeded-replay") seeded_replay();
    else if (name == "extra-senders") extra_senders();
    else if (name == "backed-up-queue") backed_up_queue();
    else throw std::runtime_error("Unknown test case: " + name);
    std::cout << "PASS " << name << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    return 1;
  }
}

#endif
