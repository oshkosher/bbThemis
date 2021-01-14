#ifndef DARSHAN_DXT_CONFLICTS_HH
#define DARSHAN_DXT_CONFLICTS_HH

#include <set>
#include <cinttypes>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
// #include <memory>


class Event {
public:
  int rank;
  enum Mode {READ, WRITE} mode;
  enum API {POSIX, MPI} api;
  int64_t offset, length;
  double start_time, end_time;

  Event() {}

  Event(int rank_) : rank(rank_) {}

  Event(int rank_, enum Mode mode_, enum API api_,
        int64_t offset_, int64_t length_,
        double start_time_, double end_time_)
    : rank(rank_), mode(mode_), api(api_), offset(offset_), length(length_),
      start_time(start_time_), end_time(end_time_) {}

  std::string str() const {
    std::ostringstream buf;
    buf << "rank " << rank
        << " bytes " << offset << ".." << (offset+length-1)
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
  Event* split(int64_t split_offset) {
    assert(split_offset >= offset && split_offset <= endOffset());
    Event *e2 = new Event(*this);
    e2->offset = split_offset;
    e2->length = this->endOffset() - split_offset;
    this->length = split_offset - offset;
    return e2;
  }


  void mergeMode(const Event &e) {
    if (e.mode != mode) {
      mode = WRITE;
    }
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

typedef std::shared_ptr<Event> EventPtr;


/*
class EventCompareEndOffset {
public:
  bool operator()(const Event &a, const Event &b) const {
    return (a.offset + a.length) < (b.offset + b.length);
  }
};
*/


/* Encapsulates all the accesses of a file made by one rank.
   It condenses a sequence of events into one nonoverlapping sequence
   of events, each of which is read-only, write-only, or read-write.

   Adjacent events of the same type are combined:
     read(10..19) + read(20..29) -> read(10..29)
   An overlapping event of a different type splits the event into
   multiple events:
     read(0..99) + write(40..49) -> read(0..39), readwrite(40..49), read(50..99)

   The start and end time of events are discarded, because they are not
   useful for determining sychronization across processors. For example,
   if a write occurs on process 0 at 10 seconds and a read occurs on process 1
   at 11 seconds, we can't assume the results of the write were visible to
   the read.

   Merge algorithm
     new:     |w-----------------------|       // a write event
     existing:    |r-----|  |w-------------|   // a read then a write
   1. Split the new event into multiple events such that no event spans the
      beginning or end of an existing event.
     new:     |w--|w-----|w-|----------|
     existing:    |r-----|  |w---------|w--|
   2. Merge overlapping events.
              |w--|rw----|w-|w---------|w--|
   3. Merge matching adjacent events.
              |w--|rw----|w----------------|

   Note that the x-axis in these diagrams represent byte ranges in the file,
   not time ranges.

   Incremental algorithm
     new:     |w-----------------------|       // a write event
     existing:    |r-----|  |w-------------|   // a read then a write
   1. Find the first existing event that might overlap; the least one
      with old.end >= new.offset.
   2. Continue adjacent. If old.end == new.offset and the types match,

   2. If new.offset < old.offset
   2. Cases:
      a) New event ends before the existing one starts. (new.end < old.offset)
             |---|  new
                   |---| existing
         Add the new one to the list. 
      b) New event ends exactly where the existing one starts
         (new.end == old.offset)
             |---|  new
                 |---| existing
         If they have the same type, extend the existing one and 
         throw away the new one. Otherwise add the new one.
      c) Left overlap
             |------|  new
                 |-----| existing
         If same type, extend existing and throw away new one.
         Otherwise, shorten both and add a third for the overlap
             |w--|rw|r-| existing
      d) Right aligned
             |r--------|  new
                 |w----| existing
         If same type, extend existing and throw away new one.
         Otherwise, shorten the new one and change the existing
         to the combined type.
             |r--|rw---|
      e) Double overlap
             |-------------|  new
                 |-----|      existing
         If same type, trim the new one to the end of the existing,
         extend the beginning of the existing, and check the next existing.
                       |---|  new
                 |-----|      existing

extend existing and throw away new one.
         Otherwise, shorten the new one and change the existing
         to the combined type.
             |r--|rw---|

         

         



*/

// Order EventPtr objects by offset
class EventPtrLT {
public:
  bool operator () (const EventPtr &a, const EventPtr &b) const {
    return a.get()->offset < b.get()->offset;
  }
};

  
class EventSequence {
public:
  typedef std::set<EventPtr, EventPtrLT> EventList;
  EventList elist;

  void addEvent_old(EventPtr &e) {
    validate();

    std::cout << "Adding " << e.get()->str() << std::endl;
    if (elist.empty()) {
      elist.insert(e);
      validate(); print();
      return;
    }
    
    // check if the initial offset of this event overlaps an existing one
    // upper bound: first element such that element > value
    //   first element that goes after
    
    // lower bound: first element such that it >= e
    EventList::iterator next_it = elist.lower_bound(e);
    assert(next_it==elist.end() || next_it->get()->offset >= e->offset);
    
    if (next_it != elist.end()) {
      std::cout << "lower bound = " << next_it->get()->str() << std::endl;
    }                

    if (next_it != elist.begin()) {
      EventList::iterator prev_it = next_it;
      prev_it--;
      Event *prev = prev_it->get();
        
      std::cout << "prev = " << prev->str() << std::endl;
      
      assert(prev->offset < e->offset);
      if (prev->endOffset() > e->offset) {

        /* e starts during prev. Split prev at e.offset and start over,
           regardless of where e.end is.
             prev  |---------|
             e1       |------|
             e2       |---------|
             e3       |---|

             prev1 |--|
             prev2    |------|
         */

        EventPtr prev_remainder(prev->split(e->offset));

        std::pair<EventList::iterator,bool> insert_result =
          elist.insert(prev_remainder);
        // check that insertion worked as expected
        assert(insert_result.second == true);
        assert(insert_result.first->get() == prev_remainder.get());

        next_it = insert_result.first;
      }
    }

    // append e
    if (next_it == elist.end()) {
      elist.insert(e);
      validate(); print();
      return;
    }

    while (e) {
      EventPtr e_remainder(nullptr);
      Event *next = next_it->get();
      assert(next);
      assert(next->offset >= e->offset);

      // next starts at or after e

      // e starts before next
      if (e->offset < next->offset) {
      
        // if there's no overlap we're done
        // XXX make a method for this, endsBefore()?
        if (e->endOffset() <= next->offset) {
          elist.insert(e);
          validate(); print();
          return;
        }

        // otherwise there's overlap and e needs to be split
        e_remainder.reset(e->split(next->offset));
        elist.insert(e);

        // continue with e_remainder, which shares a start with next
        e = e_remainder;
        e_remainder.reset();
      }
        
      assert(e->offset == next->offset);

      // if e and next are different lengths, trim the longer one
      if (e->length > next->length) {
        e_remainder.reset(e->split(next->endOffset()));
      } else if (next->length > e->length) {
        EventPtr next_remainder(next->split(e->endOffset()));
        elist.insert(next_remainder);
      }
        
      assert(e->offset == next->offset);
      assert(e->length == next->length);
      next->mergeMode(*e);

      e.reset();
      e.swap(e_remainder);
    }

    validate();
    print();
  }



  void addEvent(EventPtr &e) {
    validate();

    std::cout << "Adding " << e.get()->str() << std::endl;

    EventList::iterator overlap_it = firstOverlapping(e);
    if (overlap_it == elist.end()) {
      elist.insert(e);
      validate();
      print();
      return;
    }

    // Event *overlap = overlap_it->get();
    // Event *overlap = **overlap_it;

    if (overlap_it->get()->offset < e->offset) {
      assert(e->offset < overlap_it->get()->endOffset());
      
      /* e starts during overlap. Split off the nonoverlapping part of overlap

         overlap  |---------|
         e1          |------|
         e2          |---------|
         e3          |---|

         overlap  |--|
         overlap2    |------|
      */

      EventPtr overlap_remainder(overlap_it->get()->split(e->offset));

      std::pair<EventList::iterator,bool> insert_result =
        elist.insert(overlap_remainder);
      // check that insertion worked as expected
      assert(insert_result.second == true);
      assert(insert_result.first->get() == overlap_remainder.get());

      overlap_it = insert_result.first;
    }

    while (e) {
      EventPtr e_remainder(nullptr);
      Event *next = overlap_it->get();
      assert(next);
      assert(next->offset >= e->offset);

      // next starts at or after e

      // e starts before next
      if (e->offset < next->offset) {
      
        // if there's no overlap we're done
        if (next->startsAfter(*e)) {
          elist.insert(e);
          e.reset();
          continue;
        }

        // otherwise there's overlap and e needs to be split
        e_remainder.reset(e->split(next->offset));
        elist.insert(e);

        // continue with e_remainder, which shares a start with next
        e = e_remainder;
        e_remainder.reset();
      }

      // remaining case: e and next start at the same offset
      assert(e->offset == next->offset);

      // if e and next are different lengths, trim the longer one
      if (e->length > next->length) {
        e_remainder.reset(e->split(next->endOffset()));
      } else if (next->length > e->length) {
        EventPtr next_remainder(next->split(e->endOffset()));
        elist.insert(next_remainder);
      }
        
      assert(e->offset == next->offset);
      assert(e->length == next->length);
      next->mergeMode(*e);

      e.reset();
      e.swap(e_remainder);
    }

    validate();
    print();
  }
  

  // Returns the first event in elist that overlaps e, or elist.end()
  // if no event overlaps e.
  EventList::iterator firstOverlapping(const EventPtr &evt) {
    EventList::iterator next, prev;
    
    /* quick checks. There is no overlap if:
       - the list is empty
       - evt finishes before the first element starts
       - evt begins after the last element finishes
    */
    if (elist.empty()
        || (*elist.begin())->startsAfter(*evt)
        || evt->startsAfter(**elist.rbegin())) {
      return elist.end();
    }
    
    // get the first element such that it >= evt
    next = elist.lower_bound(evt);

    // if next is not the first element in the list, check for an overlap
    // with the element before it
    if (next != elist.begin()) {
      EventList::iterator prev = next;
      prev--;
      // if evt overlaps prev, then prev is the first overlapping event
      assert((*prev)->offset < evt->offset);
      if (!evt->startsAfter(**prev)) {
        return prev;
      }
    }
    
    // the only remaining possible overlap is that evt overlaps next
    if ((*next)->startsAfter(*evt)) {
      return elist.end();
    }

    assert(evt->overlaps(**next));
    return next;
  }

  
  bool validate() {
    // EventList::iterator it;
    const Event *prev = nullptr;

    for (EventList::iterator it = elist.begin(); it != elist.end(); it++) {
      const Event *e = it->get();
      if (prev) {
        if (e->offset <= prev->offset) {
          std::cerr << "Error out of order events (" << prev->str() << ") and ("
                    << e->str() << ")\n";
          return false;
        }

        if (e->offset < prev->endOffset()) {
          std::cerr << "Overlapping events (" << prev->str() << ") and ("
                    << e->str() << ")\n";
          return false;
        }
      }
      prev = e;
    }
    return true;
  }


  void print() {
    std::cout << "EventSequence\n";
    for (EventList::iterator it = elist.begin(); it != elist.end(); it++) {
      const Event *e = it->get();
      std::cout << "  " << e->offset << "-" << (e->endOffset()-1)
                << " " << e->str() << std::endl;
    }
  }
    

};

typedef std::unique_ptr<EventSequence> EventSequencePtr;



class File {
public:
  const std::string id;  // a hash of the filename generated by Darshan
  const std::string name;

  // rank -> EventSequence
  // this stores one EventSequence for each rank that accessed the file
  std::map<int,EventSequencePtr> rank_seq;

  EventSequence* getEventSequence(int rank) {
    std::map<int,EventSequencePtr>::iterator it = rank_seq.find(rank);
    if (it == rank_seq.end()) {
      EventSequence *seq = new EventSequence();
      rank_seq[rank] = std::unique_ptr<EventSequence>(seq);
      return seq;
    } else {
      return it->second.get();
    }
  }

  void addEvent(EventPtr &e) {
    EventSequence *seq = getEventSequence(e->rank);
    seq->addEvent(e);
  }


  // ordered by offset
  typedef std::set<Event> EventSetType;
  EventSetType events;

  File(const std::string &id_, const std::string &name_) : id(id_), name(name_) {}
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


#endif // DARSHAN_DXT_CONFLICTS_HH
