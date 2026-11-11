#ifndef SIMULATION_H
#define SIMULATION_H

#include <thread>

using namespace std;

void InitSimulation(int messages, int senders, double mean_wait_ms,
                    double mean_failure_rate, unsigned long seed,
                    double progress_interval);

void *producer();
void *sender(double failure_rate);

#endif
