#ifndef SIMULATION_H
#define SIMULATION_H

#include "message_queue.h"
#include <thread>

using namespace std;

void InitSimulation(int messages, int num_senders, double mean_wait_ms,
                    double mean_failure_rate, unsigned long seed,
                    double progress_interval);

void producer();
void sender(double send_wait, double failure_rate);

#endif
