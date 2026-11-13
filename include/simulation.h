#ifndef SIMULATION_H
#define SIMULATION_H

#include "message_queue.h"
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace std;

void InitSimulation(int messages, int num_senders, double mean_wait_ms,
                    double mean_failure_rate, unsigned long seed,
                    double progress_interval, bool progressive_print = false);

void producer();
void sender(double failure_rate, int sender_id);

#endif
