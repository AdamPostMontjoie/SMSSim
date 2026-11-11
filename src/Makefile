CXX = c++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread

sms-sim: main.cpp simulation.cpp simulation.h message_queue.h message.h
	$(CXX) $(CXXFLAGS) main.cpp simulation.cpp -o sms-sim

.PHONY: clean
clean:
	rm -f sms-sim
