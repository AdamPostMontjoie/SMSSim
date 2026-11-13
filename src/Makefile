CXX = c++
CPPFLAGS = -I../include
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread

sms-sim: main.cpp simulation.cpp message_queue.cpp ../include/simulation.h ../include/message_queue.h ../include/message.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) main.cpp simulation.cpp message_queue.cpp -o sms-sim

.PHONY: clean
clean:
	rm -f sms-sim
