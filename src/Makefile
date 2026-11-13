CXX = c++
CPPFLAGS = -I../include
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread

sms-sim: main.cpp simulation.cpp message_queue.cpp ../include/simulation.h ../include/message_queue.h ../include/message.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) main.cpp simulation.cpp message_queue.cpp -o sms-sim

simulation-tests: tests/simulation_tests.cpp simulation.cpp message_queue.cpp ../include/simulation.h ../include/message_queue.h ../include/message.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/simulation_tests.cpp message_queue.cpp -o simulation-tests

sms-sim-parser-tests: main.cpp tests/simulation_tests.cpp ../include/simulation.h ../include/message_queue.h ../include/message.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DCLI_PARSER_STUB main.cpp tests/simulation_tests.cpp -o sms-sim-parser-tests

.PHONY: test tests clean
test: sms-sim simulation-tests sms-sim-parser-tests
	./simulation-tests

tests: test

clean:
	rm -f sms-sim simulation-tests sms-sim-parser-tests
