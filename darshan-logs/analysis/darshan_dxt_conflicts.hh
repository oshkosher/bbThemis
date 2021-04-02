#ifndef DARSHAN_DXT_CONFLICTS_HH
#define DARSHAN_DXT_CONFLICTS_HH

#include <unistd.h>
#include <iostream>
#include <set>
#include <cinttypes>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <queue>
// #include <memory>

// class Event;
// using EventPtr = std::shared_ptr<Event>;

class Event {
public:
  int rank;
  enum Mode {READ, WRITE} mode;
  enum API {POSIX, MPI} api;
  int64_t offset, length;
  double start_time, end_time;

  Event() {}

  Event(int64_t offset_, int64_t length_, int mode_ = WRITE)
    : rank(0), mode((Mode)mode_), api(POSIX), offset(offset_), length(length_),
      start_time(0), end_time(0) {}

  Event(int rank_, enum Mode mode_, enum API api_,
        int64_t offset_, int64_t length_,
        double start_time_, double end_time_)
    : rank(rank_), mode(mode_), api(api_), offset(offset_), length(length_),
      start_time(start_time_), end_time(end_time_) {}

  std::string str() const {
    std::ostringstream buf;
    buf << "rank " << rank
        << " bytes " << offset << ".." << (offset+length)
        << " " << (api==POSIX ? "POSIX" : "MPIIO")
        << " " << (mode==READ ? "read" : "write")
        << " time " << std::fixed << std::setprecision(4) << start_time << ".." << end_time;
    return buf.str();
  }

  // return the offset after the last byte of this access
  int64_t endOffset() const {return offset + length;}


  // this event starts after the given event finishes
  bool startsAfter(const Event &x) const {
    return offset >= x.endOffset();
  }


  // Split this event into two (offset..split_offset), (split_offset..end)
  // Return the second one leaving this one's offset unchanged.
  Event split(int64_t split_offset) {
    assert(split_offset >= offset && split_offset <= endOffset());
    Event e2(*this);
    e2.offset = split_offset;
    e2.length = this->endOffset() - split_offset;
    this->length = split_offset - offset;
    return e2;
  }


  void mergeMode(const Event &e) {
    if (e.mode != mode) {
      mode = WRITE;
    }
  }


  /* Returns true iff e is identical and adjacent (after) this event */
  bool canExtend(const Event &e) {
    return rank == e.rank
      && mode == e.mode
      && endOffset() == e.offset;
  }


  // check if e's byte range and timespan are a superset of mine
  bool isParentEvent(const Event &e) const {
    return e.offset <= offset
      && e.endOffset() >= endOffset()
      && e.start_time <= start_time
      && e.end_time >= end_time
      && e.api == MPI
      && api == POSIX;
  }


  // merge 'other' into this event. one must be an MPI event and the other POSIX
  void merge(const Event &e) {
    if (api == e.api) {
      std::cerr << "Unexpected overlap of IO accesses from same rank:\n"
                << "  " << this->str() << "\n  " << e.str() << std::endl;
      return;
    }

    if ((e.api == MPI && !isParentEvent(e)) ||
        (api == MPI && !e.isParentEvent(*this))) {
      std::cerr << "Ambiguous parentage of overlapping events from the same rank:\n"
                << "  " << this->str() << "\n  " << e.str() << std::endl;
      return;
    }

    api = MPI;
    // upgrade to WRITE if new event is a write
    if (e.mode == WRITE) mode = WRITE;
    int64_t tmp_o = std::min(offset, e.offset);
    length = std::max(endOffset(), e.endOffset()) - tmp_o;
    offset = tmp_o;
    start_time = std::min(start_time, e.start_time);
    end_time = std::max(end_time, e.end_time);
  }


  bool overlaps(const Event &other) const {
    return (offset < other.endOffset())
      && (endOffset() > other.offset);
  }

  /* If all accesses are done in terms of blocks of data, set this to
     the block size so overlaps can be computed correctly.

     For example, let block_size be 100. Then every read or write to disk
     occurs in blocks of 100 bytes. If P0 wants to overwrite bytes 0..3, it
     will need to read bytes 0..99 from disk, overwrite the first four bytes,
     then write bytes 0..99 to disk. If P1 writes bytes 96..99 with no
     synchronization, it may complete its operation after P0 read the block
     and before P0 wrote the block. Then when P0 writes its block, it will
     overwrite P1's changes.

     AFAICT, this will only be an issue in write-after-write (WAW) situations.
     In RAW or WAR situations, if the byte range doesn't actually overlap, the
     read will get the same result whether preceding write completed or not.
  */
  static int block_size;
  static void setBlockSize(int b) {block_size = b;}

  bool overlapsBlocks(const Event &other) const {
    int64_t this_start, this_end, other_start, other_end;

    this_start = blockStart(offset);
    this_end = blockEnd(endOffset() - 1);

    other_start = blockStart(other.offset);
    other_end = blockEnd(other.endOffset() - 1);

    return (this_start < other_end) && (this_end > other_start);
  }

  // round down an offset to the beginning of a block
  static int64_t blockStart(int64_t offset) {
    return offset - (offset % block_size);
  }

  // round up an offset to the end of a block
  static int64_t blockEnd(int64_t offset) {
    return blockStart(offset) + block_size - 1;
  }

  // order by offset and then start time
  bool operator < (const Event &that) const {
    if (offset == that.offset) {
      return start_time < that.start_time;
    } else {
      return offset < that.offset;
    }
  }
};


// Order EventPtr objects by offset
class EventsOrderByOffset {
public:
  bool operator () (const Event &a, const Event &b) const {
    return a.offset < b.offset;
  }
};


struct SeqEvent {
  int64_t offset, length;
  Event::Mode mode;

  SeqEvent() : offset(0), length(0), mode(Event::Mode::READ) {}
  
  SeqEvent(const Event &e)
    : offset(e.offset), length(e.length), mode(e.mode) {}

  int64_t endOffset() const {return offset + length;}

  std::string str() const {
    std::ostringstream buf;
    buf << (mode==Event::Mode::READ ? "read" : "write")
        << " " << offset << ".." << (offset+length);
    return buf.str();
  }    
  
  
  // this event starts after the given event finishes
  bool startsAfter(const SeqEvent &x) const {
    return offset >= x.endOffset();
  }


  bool overlaps(const SeqEvent &other) const {
    return (offset < other.endOffset())
      && (endOffset() > other.offset);
  }


  /* Returns true iff e is identical and adjacent (after) this event */
  bool canExtend(const SeqEvent &e) {
    return mode == e.mode
      && endOffset() == e.offset;
  }


  void mergeMode(const SeqEvent &e) {
    if (e.mode != mode) {
      mode = Event::Mode::WRITE;
    }
  }


  // Split this event into two (offset..split_offset), (split_offset..end)
  // Return the second one leaving this one's offset unchanged.
  SeqEvent split(int64_t split_offset) {
    assert(split_offset >= offset && split_offset <= endOffset());
    SeqEvent e2(*this);
    e2.offset = split_offset;
    e2.length = this->endOffset() - split_offset;
    this->length = split_offset - offset;
    return e2;
  }
};


class EventSequence {
  
public:
  // map offset to Event
  using EventList = std::map<int64_t, SeqEvent>;

  EventSequence(std::string name_ = "") : name(name_) {}

  const std::string& getName() {return name;}
  
  void addEvent(const Event &e);
  
  bool validate();
  void print();

  // join adjacent events with matching types
  void minimize();

  // remove all events
  void clear() {elist.clear();}

  size_t size() const {return elist.size();}
  EventList::const_iterator begin() const {return elist.begin();}
  EventList::const_iterator end() const {return elist.end();}

private:
  EventList elist;
  std::string name;

  // Returns the first event in elist that overlaps e, or elist.end()
  // if no event overlaps e.
  EventList::iterator firstOverlapping(const SeqEvent &evt);

  // insert e.offset->e, and return an iterator to the destination
  EventList::iterator insert(const SeqEvent &e) {
    return elist.insert( std::pair<int64_t,SeqEvent> (e.offset, e) ).first;
  }
};

using EventSequencePtr = std::unique_ptr<EventSequence>;


class LineReader {
  long lines_read, next_report, report_freq;
  bool do_report;

public:
  LineReader(long report_freq_) : lines_read(0), next_report(report_freq_),
                                  report_freq(report_freq_) {
    do_report = (isatty(STDERR_FILENO) == 1);
  }

  bool getline(std::istream &in, std::string &line) {
    if (std::getline(in, line)) {
      lines_read++;
      if (do_report) {
        if (lines_read >= next_report) {
          std::cerr << "\r" << lines_read << " lines read";
          std::cerr.flush();
          next_report = lines_read + report_freq;
        }
      }
      return true;
    } else {
      return false;
    }
  }

  void done() {
    if (do_report) {
      std::cerr << "\r" << lines_read << " lines read" << std::endl;
    }
  }
};
    


class File {
public:
  const std::string id;  // a hash of the filename generated by Darshan
  const std::string name;

  
  // rank -> EventSequence
  // this stores one EventSequence for each rank that accessed the file
  using RankSeqMap = std::map<int,EventSequencePtr>;
  // using RankSeqIter = std::map<int,EventSequencePtr>::const_iterator;
  
  RankSeqMap rank_seq;

  EventSequence* getEventSequence(int rank) {
    std::map<int,EventSequencePtr>::iterator it = rank_seq.find(rank);
    if (it == rank_seq.end()) {
      std::string seq_name = std::string("rank=") + std::to_string(rank)
        + " filename=" + name;
      EventSequence *seq = new EventSequence(seq_name);
      rank_seq[rank] = std::unique_ptr<EventSequence>(seq);
      return seq;
    } else {
      return it->second.get();
    }
  }

  void addEvent(const Event &e) {
    EventSequence *seq = getEventSequence(e.rank);
    seq->addEvent(e);
  }


  // ordered by offset
  typedef std::set<Event> EventSetType;
  EventSetType events;

  File(const std::string &id_, const std::string &name_) : id(id_), name(name_) {}
};


class RankSeq {
  const int rank_;
  const EventSequence *seq_;
  EventSequence::EventList::const_iterator it_;

public:

  // RankSeqIter
  RankSeq(File::RankSeqMap::const_iterator it)
    : rank_(it->first), seq_(it->second.get()), it_(seq_->begin()) {}

  RankSeq(int rank, EventSequence *seq)
    : rank_(rank), seq_(seq), it_(seq_->begin()) {}
  
  int rank() const {return rank_;}
  // EventSequence* sequence() const {return seq_;}
  bool done() const {return it_ == seq_->end();}
  bool next() {
    if (done()) return false;
    it_++;
    return !done();
  }
  const SeqEvent &event() const {return it_->second;}

  int64_t offset() const {
    if (done()) {
      return INT64_MAX;
    } else {
      return event().offset;
    }
  }

  int64_t endOffset() const {
    if (done()) {
      return INT64_MAX;
    } else {
      return event().endOffset();
    }
  }

  struct OrderByOffset {
    bool operator() (RankSeq *a, RankSeq *b) {
      return a->offset() > b->offset();
    }
  };

  struct OrderByEndOffset {
    bool operator() (RankSeq *a, RankSeq *b) {
      return a->endOffset() > b->endOffset();
    }
  };
};





/* Merge a set of sequences of ranges into a sequence of subranges where the 
   set of active ranks and their mode (read or write) is constant.
   For example, with the following set of sequences:
     rank 0: read(10..20)
     rank 1: read(30..100)
     rank 2: write(50..200)
   the subranges would be:
     10..20: 0(read)
     20..30: 
     30..50: 1(read)
     50..100: 1(read), 2(write)
     100..200: 2(write)
   
   Starting after the first call to next(), the user can query the bounds
   of the current range with getRangeStart() and getRangeEnd(), and the
   user can query the set of ranks and their modes with getActiveSet().
*/
class RangeMerge {
public:
  using ActiveSet = std::map<int,Event::Mode>;

private:
  std::vector<RankSeq> ranks;
  int64_t range_start, range_end;

  // rank -> Event::Mode
  ActiveSet active_set;
  
  std::priority_queue<RankSeq*, std::vector<RankSeq*>, RankSeq::OrderByOffset>
    incoming_queue;
  std::priority_queue<RankSeq*, std::vector<RankSeq*>, RankSeq::OrderByEndOffset>
    outgoing_queue;
  
  
public:
  RangeMerge(File::RankSeqMap &rank_sequences);

  // move to the next range. Returns false iff there are no more ranges.
  bool next();

  int64_t getRangeStart() {return range_start;}
  int64_t getRangeEnd() {return range_end;}
  const ActiveSet& getActiveSet() {return active_set;}
};


class OverlapSet {
private:
  std::vector<Event> events;

public:
  // remove elements whose end offset is less than end_offset
  void removeOldEvents(int64_t end_offset) {
    size_t i = 0;
    while (i < events.size()) {
      if (events[i].offset + events[i].length <= end_offset) {
        events[i] = events[events.size()-1];
        events.resize(events.size()-1);
      } else {
        i++;
      }
    }
  }

  // returns true if the new event was merged into an existing event
  bool mergeEventsSameRank(const Event &new_event) {
    for (Event &e : events) {
      if (e.rank == new_event.rank) {
        e.merge(new_event);
        return true;
      }
    }
    return false;
  }

  std::string hazardType(const Event &first, const Event &second) {
    if (first.mode == Event::READ) {
      if (second.mode == Event::READ) {
        return "RAR";
      } else {
        return "WAR";
      }
    } else {
      if (second.mode == Event::READ) {
        return "RAW";
      } else {
        return "WAW";
      }
    }
  }

  void reportOverlaps(const Event &e2) {
    for (Event &e1 : events) {
      if (e1.overlaps(e2) &&
          (e1.mode == Event::WRITE || e2.mode == Event::WRITE)) {
        const Event &first = (e1.start_time < e2.start_time) ? e1 : e2;
        const Event &second = (e1.start_time >= e2.start_time) ? e1 : e2;

        std::cout << hazardType(first, second) << " hazard.\n  "
                  << first.str() << "\n  " << second.str() << std::endl;

      }
    }
  }


  void reportBlockOverlaps(const Event &e2) {
    for (Event &e1 : events) {
      if (!e1.overlaps(e2) &&
          e1.overlapsBlocks(e2) &&
          e1.mode == Event::WRITE &&
          e2.mode == Event::WRITE) {
        const Event &first = (e1.start_time < e2.start_time) ? e1 : e2;
        const Event &second = (e1.start_time >= e2.start_time) ? e1 : e2;

        std::cout << "WAW false sharing hazard.\n  "
                  << first.str() << "\n  " << second.str() << std::endl;

      }
    }
  }

  void addEvent(const Event &e) {
    events.push_back(e);
  }

};


// split a line by tab characters
void splitTabString(std::vector<std::string> &fields, const std::string &line);


#endif // DARSHAN_DXT_CONFLICTS_HH
