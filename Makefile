CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic

SCHED_SRC = scheduler.cpp generator.cpp
SCHED_HDR = scheduler.hpp generator.hpp

.PHONY: all tests stress clean

all: tests_run stress_run

tests_run: $(SCHED_SRC) $(SCHED_HDR) tests.cpp
	$(CXX) $(CXXFLAGS) $(SCHED_SRC) tests.cpp -o tests_run

stress_run: $(SCHED_SRC) $(SCHED_HDR) stress_test.cpp
	$(CXX) $(CXXFLAGS) $(SCHED_SRC) stress_test.cpp -o stress_run

tests: tests_run
	./tests_run

stress: stress_run
	./stress_run

clean:
	rm -f tests_run stress_run
