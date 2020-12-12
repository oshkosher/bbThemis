default: all

EXECS = darshan_dxt_conflicts
all: $(EXECS)

CXX = g++ -std=c++11 -Wall -g


darshan_dxt_conflicts: darshan_dxt_conflicts.cc
	$(CXX) $< -o $@

clean:
	rm -f $(EXECS) *.exe
