default: all

EXECS = darshan_dxt_conflicts
all: $(EXECS)

CXX = g++ -std=c++11 -Wall -O3

darshan_dxt_conflicts: darshan_dxt_conflicts.cc darshan_dxt_conflicts.hh
	$(CXX) $< -o $@

darshan_dxt_conflicts.test: darshan_dxt_conflicts.cc darshan_dxt_conflicts.hh
	$(CXX) -DTESTING $< -o $@

clean:
	rm -f $(EXECS) *.exe *.stackdump
