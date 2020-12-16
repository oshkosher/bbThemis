default: all

EXECS = darshan_dxt_conflicts
all: $(EXECS)

CXX = g++ -std=c++11 -Wall -g


darshan_dxt_conflicts: darshan_dxt_conflicts.cc darshan_dxt_conflicts.hh
	$(CXX) $< -o $@

clean:
	rm -f $(EXECS) *.exe *.stackdump
