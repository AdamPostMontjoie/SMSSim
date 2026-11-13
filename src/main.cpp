#include "simulation.h"
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

using namespace std;

int main(int argc, char *argv[]) {
    int messages = 1000;
    int senders = 4;
    double mean_wait_ms = 100.0;
    double mean_failure_rate = 0.1;
    double progress_interval = 1.0;
    unsigned long seed = 0;
    bool has_seed = false;
    bool progressive_print = false;

    for (int i = 1; i < argc; i++) {
        string option = argv[i];
        if (option == "--help") {
            cout << "./sms-sim [options]\n"
                 << "  --messages NUMBER   Message count (default: 1000)\n"
                 << "  --senders NUMBER    Sender count (default: 4)\n"
                 << "  --mean-wait-ms NUMBER       Mean send wait in milliseconds (default: 100)\n"
                 << "  --mean-failure-rate NUMBER  Mean failure probability, 0 to 1 (default: 0.1)\n"
                 << "  --seed NUMBER              Random seed (default: automatically generated)\n"
                 << "  --progress-interval NUMBER  Progress update interval in seconds (default: 1)\n"
                 << "  --progressive-print        Keep progress updates on separate lines (default: off)\n";
            return 0;
        }
        if (option == "--progressive-print") {
            progressive_print = true;
            continue;
        }
        if (option != "--messages" && option != "--senders" &&
            option != "--mean-wait-ms" && option != "--mean-failure-rate" &&
            option != "--seed" && option != "--progress-interval") {
            cerr << "Unknown option: " << option << endl;
            return 1;
        }
        if (i + 1 >= argc) {
            cerr << "Missing value after " << option << endl;
            return 1;
        }
        i++; // Move from the flag to its value.
        if (option == "--messages") {
            try {
                messages = stoi(argv[i]);
            } catch (const out_of_range &) {
                cerr << "Message count must be between 0 and "
                     << numeric_limits<int>::max() << ".\n";
                return 1;
            }
        } else if (option == "--senders") {
            senders = stoi(argv[i]);
        } else if (option == "--mean-wait-ms") {
            mean_wait_ms = stod(argv[i]);
        } else if (option == "--mean-failure-rate") {
            mean_failure_rate = stod(argv[i]);
        } else if (option == "--seed") {
            seed = stoul(argv[i]);
            has_seed = true;
        } else if (option == "--progress-interval") {
            progress_interval = stod(argv[i]);
        }
    }

    if (messages < 0 || senders <= 0) {
        cerr << "Messages must be >= 0 and senders must be > 0.\n";
        return 1;
    }
    if (mean_wait_ms < 0 || mean_failure_rate < 0 || mean_failure_rate > 1 ||
        progress_interval <= 0) {
        cerr << "Mean wait must be >= 0, mean failure rate must be between 0 and 1, "
             << "and progress interval must be > 0.\n";
        return 1;
    }
    if (!has_seed) {
        seed = random_device{}();
    }
    cout << "Messages: " << messages << endl;
    cout << "Senders: " << senders << endl;
    cout << "Mean send wait (ms): " << mean_wait_ms << endl;
    cout << "Mean failure rate: " << mean_failure_rate << endl;
    cout << "Seed: " << seed << endl;
    cout << "Progress interval (seconds): " << progress_interval << endl;

    InitSimulation(messages, senders, mean_wait_ms, mean_failure_rate, seed,
                   progress_interval, progressive_print);

    return 0;
}
