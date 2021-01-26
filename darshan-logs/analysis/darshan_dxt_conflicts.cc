/*
  darshan_dxt_conflicts - reads the output of darshan-dxt-parser (which
  contains per-call data on each read or write) and outputs any conflicts
  found.

  A conflict is when a pair of events A and B are found such that:
   - A and B access the same file (A.file_hash == B.file_hash)
   - A and B came from different processes (A.rank != B.rank)
   - A and B access overlapping byte ranges
     (A.offset < (B.offset+B.length) && ((A.offset+A.length) > B.offset)
   - At least one of the accesses is a write. (A.mode==WRITE || B.mode==WRITE)


  Sample input:

# DXT, file_id: 8515199880342690440, file_name: /mnt/c/Ed/UI/Research/Darshan/conflict_app.out.RAW.POSIX.NONE
# DXT, rank: 0, hostname: XPS13
# DXT, write_count: 10, read_count: 0
# DXT, mnt_pt: /mnt/c, fs_type: 9p
# Module    Rank  Wt/Rd  Segment          Offset       Length    Start(s)      End(s)
 X_POSIX       0  write        0               0         1048576      4.8324      4.8436
 X_POSIX       0  write        1         1048576         1048576      4.8436      4.8534
 X_POSIX       0  write        2         2097152         1048576      4.8534      4.8637
 X_POSIX       0  write        3         3145728         1048576      4.8638      4.8706
 X_POSIX       0  write        4         4194304         1048576      4.8706      4.8769
 X_POSIX       0  write        5         5242880         1048576      4.8769      4.8832
 X_POSIX       0  write        6         6291456         1048576      4.8832      4.8892
 X_POSIX       0  write        7         7340032         1048576      4.8892      4.8944
 X_POSIX       0  write        8         8388608         1048576      4.8944      4.9000
 X_POSIX       0  write        9         9437184         1048576      4.9000      4.9059

# DXT, file_id: 8515199880342690440, file_name: /mnt/c/Ed/UI/Research/Darshan/conflict_app.out.RAW.POSIX.NONE
# DXT, rank: 1, hostname: XPS13
# DXT, write_count: 0, read_count: 10
# DXT, mnt_pt: /mnt/c, fs_type: 9p
# Module    Rank  Wt/Rd  Segment          Offset       Length    Start(s)      End(s)
 X_POSIX       1   read        0               0         1048576      6.8327      6.8392
 X_POSIX       1   read        1         1048576         1048576      6.8392      6.8434
 X_POSIX       1   read        2         2097152         1048576      6.8434      6.8473
 X_POSIX       1   read        3         3145728         1048576      6.8473      6.8513
 X_POSIX       1   read        4         4194304         1048576      6.8513      6.8553
 X_POSIX       1   read        5         5242880         1048576      6.8553      6.8601
 X_POSIX       1   read        6         6291456         1048576      6.8601      6.8639
 X_POSIX       1   read        7         7340032         1048576      6.8639      6.8673
 X_POSIX       1   read        8         8388608         1048576      6.8673      6.8709
 X_POSIX       1   read        9         9437184         1048576      6.8709      6.8747

*/

#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <memory>
#include <regex>

#include "darshan_dxt_conflicts.hh"

using namespace std;


int Event::block_size = 1;


// map file_id (the hash of the file path) to File object.
// Use the hash rather than the path, because the path is
// often truncated in Darshan, leading to collisions that would probably
// be avoided when using the 64-bit hash of the full path.
// typedef unordered_map<std::string, unique_ptr<File>> FileTableType;
typedef map<std::string, unique_ptr<File>> FileTableType;

int readDarshanDxtInput(istream &in, FileTableType &file_table,
                        LineReader &line_reader);
bool parseEventLine(Event &e, const string &line);
void writeData(const FileTableType &file_table);
void scanForConflicts(File *f);
void testEventSequence();


int main(int argc, char **argv) {
  FileTableType file_table;

#if TESTING
#undef NDEBUG
  testEventSequence();
  return 0;
#endif

  if (argc == 1) {
    cerr << "\n  darshan_dxt_conflicts <input_dxt_file> [...]\n\n";
    exit(1);
  }

  LineReader line_reader(5000);
  for (int argno=1; argno < argc; argno++) {
    ifstream inf(argv[argno]);
    readDarshanDxtInput(inf, file_table, line_reader);
  }
  line_reader.done();

  // scan files in name order
  vector<File*> files_by_name;
  for (auto &file_it : file_table) {
    files_by_name.push_back(file_it.second.get());
  }
  sort(files_by_name.begin(), files_by_name.end(),
       [](File *a, File *b) {return a->name < b->name;});
  
  for (File *f : files_by_name) {
    scanForConflicts(f);
  }

  
  return 0;
}


int readDarshanDxtInput(istream &in, FileTableType &file_table,
                        LineReader &line_reader) {
  string line;

  regex section_header_re("^# DXT, file_id: ([0-9]+), file_name: (.*)$");
  regex rank_line_re("^# DXT, rank: ([0-9]+),");
  // regex io_event_re("^ *(X_MPIIO|X_POSIX) +[:digit:]+ +([:alpha:]+)");
  smatch re_matches;
  
  while (true) {

    // skip until the beginning of a section is found
    bool section_found = false;
    while (true) {
      if (!line_reader.getline(in, line)) break;
      if (regex_search(line, re_matches, section_header_re)) {
        section_found = true;
        break;
      }
    }
    if (!section_found) break;
    
    assert(re_matches.size() == 3);
    string file_id_str = re_matches[1];
    string file_name = re_matches[2];

    File *current_file;
    FileTableType::iterator ftt_iter = file_table.find(file_id_str);
    if (ftt_iter == file_table.end()) {
      // cout << "First instance of " << file_name << endl;
      current_file = new File(file_id_str, file_name);
      file_table[file_id_str] = unique_ptr<File>(current_file);
    } else {
      current_file = ftt_iter->second.get();
    }
    
    // find the line with the rank id
    bool rank_found = false;
    while (true) {
      if (!line_reader.getline(in, line)) break;
      if (regex_search(line, re_matches, rank_line_re)) {
        rank_found = true;
        break;
      }
    }
    if (!rank_found) break;

    // int rank = stoi(re_matches[1]);

    // cout << "reading rank " << rank << " " << file_name << endl;

    // read until a blank line at the end of the section or EOF
    bool is_eof = false;
    while (true) {
      if (!line_reader.getline(in, line)) break;
      if (line.length() == 0) {
        // cout << "End of section\n";
        break;
      }
      if (line[0] == '#') continue;
      
      Event event;
      if (!parseEventLine(event, line)) {
        cerr << "Unrecognized line: " << line << endl;
      } else {
        // cout << event.str() << endl;

        // ignore events with an invalid offset
        if (event.offset >= 0) {
          current_file->addEvent(event);
        }

      }
    }

    if (is_eof) break;
  }

  // cout << "Reading done.\n";
  
  for (auto file_it = file_table.begin();
       file_it != file_table.end(); file_it++) {
    File *file = file_it->second.get();
    // cout << "File " << file->name << endl;
    for (auto rank_seq_it = file->rank_seq.begin();
         rank_seq_it != file->rank_seq.end(); rank_seq_it++) {
      // cout << "  rank " << rank_seq_it->first << endl;
      EventSequence *seq = rank_seq_it->second.get();
      // seq->print();
      // cout << "    minimizing\n";
      seq->minimize();
      // seq->print();
    }
  }
    

  return 0;
}


/*
bool startsWith(const string &s, const char *search_str) {
  size_t search_len = strlen(search_str);
  return s.length() >= search_len
    && 0 == s.compare(0, search_len, search_str);
}
*/


/* 
   Parse a line in the form:
      X_POSIX   1  read    9    4718592     524288   1.2240  1.2261
   Subexpressions:
   1: io library (X_MPIIO or X_POSIX)
   2: rank
   3: direction (write or read)
   4: offset
   5: length
   6: start time
   7: end time
*/
static regex io_event_re("^ *(X_MPIIO|X_POSIX) +([0-9]+) +([a-z]+) +[0-9]+ +([-0-9]+) +([0-9]+) +([0-9.]+) +([0-9.]+)");

bool parseEventLine(Event &event, const string &line) {
  smatch re_matches;

  if (!regex_search(line, re_matches, io_event_re)) return false;
  if (re_matches.size() != 8) return false;

  /*
  cout << "  " << re_matches[1] << " action=" << re_matches[2]
       << " offset=" << re_matches[3]
       << " length=" << re_matches[4]
       << " start=" << re_matches[5]
       << " end=" << re_matches[6]
       << endl;
  */
  
  if (re_matches[1] == "X_POSIX") {
    event.api = Event::POSIX;
  } else if (re_matches[1] == "X_MPIIO") {
    event.api = Event::MPI;
  } else {
    cerr << "invalid library: " << re_matches[1] << endl;
    return false;
  }

  event.rank = stoi(re_matches[2]);

  if (re_matches[3] == "read") {
    event.mode = Event::READ;
  } else if (re_matches[3] == "write") {
    event.mode = Event::WRITE;
  } else {
    cerr << "invalid io access type: " << re_matches[3] << endl;
    return false;
  }

  event.offset = stoll(re_matches[4]);
  event.length = stoll(re_matches[5]);
  event.start_time = stod(re_matches[6]);
  event.end_time = stod(re_matches[7]);

  return true;
}


void writeData(const FileTableType &file_table) {

  for (auto &file_it : file_table) {
    File *f = file_it.second.get();
    cout << "File " << f->name << endl;
    for (auto &event_it : f->events) {
      cout << event_it.str() << endl;
    }
  }
  /*
  for (FileTableType::const_iterator it=file_table.begin();
       it != file_table.end(); it++) {
    const File *f = it->second.get();
    cout << "File " << f->name << endl;
  }
  */
}


/* Scan through the events, looking for instances where multiple ranks
   accessed the same bytes and at least one of the accesses was a write.

   The data is an EventSequence for each rank, which is an ordered list of
   nonoverlapping ranges of reads or writes.  For example:
     rank 0:  read 1..100, write 100-200, read 200-300
     rank 1:  read 50..250
     rank 2:  write 120-140, write 220-240, write 400-500

   Loop through the bytes of the file, maintaining a set of all the ranks
   that accessed the current range of the file. Use two priority queues to
   track extents that are starting and ending:
     outgoing min-heap, ordered by endOffset
       root is the next extent to end
     incoming min-heap, ordered by offset
       root is the next extent to start
*/
void scanForConflicts(File *f) {
  cout << "\nscanForConflicts(" << f->name << ")\n";
  if (f->name == "<STDERR>" || f->name == "<STDOUT>") {
    cout << "  ignored\n";
    return;
  }

  RangeMerge range_merge(f->rank_seq);

  while (range_merge.next()) {
    const RangeMerge::ActiveSet &active = range_merge.getActiveSet();

    bool any_writes = false;
    for (auto &it : active) {
      if (it.second==Event::WRITE) {
        any_writes = true;
        break;
      }
    }

    if (active.size() > 1 && any_writes) {
      cout << "CONFLICT\n  " << range_merge.getRangeStart() << ".."
           << range_merge.getRangeEnd() << ":";
      for (auto &it : active) {
        cout << " " << it.first << "("
             << (it.second==Event::READ ? "read" : "write")
             << ")";
        
      }
      cout << endl;
    }
    
  }
  
}



void EventSequence::addEvent(const Event &full_event) {
  // assert(validate());

  // if (full_event.offset < 0) return;
  
  SeqEvent e(full_event);

  // std::cout << "Adding " << e.str() << std::endl;

  EventList::iterator overlap_it = firstOverlapping(e);
  if (overlap_it == elist.end()) {
    insert(e);
    // assert(validate());
    return;
  }

  if (overlap_it->second.offset < e.offset) {
    assert(e.offset < overlap_it->second.endOffset());
      
    /* e starts during overlap. Split off the nonoverlapping part of overlap

       overlap  |---------|
       e1          |------|
       e2          |---------|
       e3          |---|

       overlap  |--|
       overlap2    |------|
    */

    SeqEvent overlap_remainder = overlap_it->second.split(e.offset);

    overlap_it = insert(overlap_remainder);
  }

  EventList::iterator next_it = overlap_it;

  // e is a new event we're adding to elist
  // next_it points to the first event in elist that starts at the same offset
  // as e or later.
  while (true) {

    // if e is past the end of existing elements just insert it
    if (next_it == elist.end()) {
      insert(e);
      break;
    }
    
    SeqEvent &next = next_it->second;
    
    // if there's no overlap we're done
    if (next.startsAfter(e)) {
      insert(e);
      break;
    }
    
    assert(next.offset >= e.offset);
    if (!(next.offset < e.endOffset())) {
      cout << "Error !(next.offset < e.endOffset()), next=" <<
        next.str() << ", e=" << e.str() << endl;
    }
    assert(next.offset < e.endOffset());

    // e starts before next: split off the prefix of e
    if (e.offset < next.offset) {

      // we know next doesn't start after e, so there's an overlap
      // and e needs to be split
      SeqEvent tmp = e.split(next.offset);
      insert(e);

      // continue with the remainder, which shares a start with next
      e = tmp;
    }

    // remaining case: e and next start at the same offset
    assert(e.offset == next.offset);

    SeqEvent e_leftover;
    
    // if e and next are different lengths, trim the longer one
    if (e.length > next.length) {
      // e is longer, merge the beginnging into next
      e_leftover = e.split(next.endOffset());
    } else if (next.length > e.length) {
      // next is longer, split it, merge e, and finish
      insert(next.split(e.endOffset()));
    }
    
    assert(e.offset == next.offset);
    assert(e.length == next.length);
    next.mergeMode(e);

    /* e longer than next: split e, merge, continue with remainder of e
       next longer than e: split next, merge, done
       same length: merge, done */
    
    if (e_leftover.length == 0)
      break;

    e = e_leftover;
    next_it++;
  }

  // assert(validate());
}


EventSequence::EventList::iterator EventSequence::firstOverlapping
(const SeqEvent &evt) {
  EventList::iterator next, prev;
    
  /* quick checks. There is no overlap if:
     - the list is empty
     - evt finishes before the first element starts
     - evt begins after the last element finishes
  */
  if (elist.empty()
      || elist.begin()->second.startsAfter(evt)
      || evt.startsAfter(elist.rbegin()->second)) {
    return elist.end();
  }
    
  // get the first element such that it >= evt
  next = elist.lower_bound(evt.offset);

  // if next is not the first element in the list, check for an overlap
  // with the element before it
  if (next != elist.begin()) {
    EventList::iterator prev = next;
    prev--;
    // if evt overlaps prev, then prev is the first overlapping event
    assert(prev->first < evt.offset);
    if (!evt.startsAfter(prev->second)) {
      return prev;
    }
  }
    
  // the only remaining possible overlap is that evt overlaps next
  if (next->second.startsAfter(evt)) {
    return elist.end();
  }

  assert(evt.overlaps(next->second));
  return next;
}


bool EventSequence::validate() {
  if (elist.empty()) return true;;
  
  EventList::iterator prev_it = elist.begin();
  EventList::iterator it = prev_it;
  assert(it->first == it->second.offset);
  it++;

  while (it != elist.end()) {

    const SeqEvent &prev = prev_it->second, &e = it->second;
    assert(prev.offset == prev_it->first);
    assert(e.offset == it->first);

    if (e.offset <= prev.offset) {
      std::cerr << "Error out of order events (" << prev.str() << ") and ("
                << e.str() << ")\n";
      return false;
    }

    if (e.offset < prev.endOffset()) {
      std::cerr << "Overlapping events (" << prev.str() << ") and ("
                << e.str() << ")\n";
      return false;
    }

    prev_it = it;
    it++;
  }

  return true;
}


void EventSequence::print() {
  std::cout << "EventSequence " << getName() << std::endl;
  for (EventList::const_iterator it = begin(); it != end(); it++) {
    const SeqEvent &e = it->second;
    /* std::cout << "  " << e.offset << "-" << e.endOffset()
       << " " << e.str() << std::endl; */
    std::cout << "  " << e.str() << std::endl;
  }
}


void EventSequence::minimize() {
  if (elist.size() <= 1) return;

  assert(validate());
  EventList::iterator it = elist.begin();

  while (true) {
    EventList::iterator next = it;
    next++;
    if (next == elist.end()) break;

    SeqEvent &e = it->second, &n = next->second;
    if (e.canExtend(n)) {
      e.length += n.length;
      elist.erase(next);
    } else {
      it = next;
    }
  }
  assert(validate());
}


static void initSequence(EventSequence &s,
                         const vector<int64_t> &bound_pairs) {
  s.clear();
  for (size_t i=0; i < bound_pairs.size(); i+= 2) {
    s.addEvent(Event(bound_pairs[i], bound_pairs[i+1] - bound_pairs[i]));
  }
}

static void initSequence2(EventSequence &s,
                          const vector<int64_t> &bound_pairs) {
  s.clear();
  for (size_t i=0; i < bound_pairs.size(); i+= 3) {
    s.addEvent(Event(bound_pairs[i], bound_pairs[i+1] - bound_pairs[i],
                     bound_pairs[i+2]));
  }
}

static void checkSequence(const EventSequence &s,
                          const vector<int64_t> &bound_pairs) {
  size_t i = 0;
  assert(s.size() == bound_pairs.size()/2);
  EventSequence::EventList::const_iterator it = s.begin();
  while (it != s.end()) {
    assert(it->first == bound_pairs[i]
           && it->second.endOffset() == bound_pairs[i+1]);
    i += 2;
    it++;
  }
}

static void checkSequence2(const EventSequence &s,
                           const vector<int64_t> &bound_pairs) {
  size_t i = 0;
  assert(s.size() == bound_pairs.size()/3);
  EventSequence::EventList::const_iterator it = s.begin();
  while (it != s.end()) {
    assert(it->first == bound_pairs[i]
           && it->second.endOffset() == bound_pairs[i+1]);
    assert(it->second.mode == (Event::Mode)bound_pairs[i+2]);
    i += 3;
    it++;
  }
}
  


void testEventSequence() {
  EventSequence s;
  EventSequence::EventList::iterator it;

  // |rrrrrr|
  //    |wwwwwww|
  {
    vector<int64_t> in {10, 60, Event::READ, 20, 70, Event::WRITE};
    vector<int64_t> out {10, 20, Event::READ, 20, 60, Event::WRITE,
                          60, 70, Event::WRITE};
    initSequence2(s, in);
    checkSequence2(s, out);
  }

  // |wwwwwww|
  //    |rrrrrrrr|
  {
    vector<int64_t> in {10, 60, Event::WRITE, 20, 70, Event::READ};
    vector<int64_t> out {10, 20, Event::WRITE, 20, 60, Event::WRITE,
                          60, 70, Event::READ};
    initSequence2(s, in);
    checkSequence2(s, out);
  }

  // |------|
  //    |---|
  {
    vector<int64_t> in {10, 50, 20, 50};
    vector<int64_t> out {10, 20, 20, 50};
    initSequence(s, in);
    checkSequence(s, out);
    vector<int64_t> out2 {10, 50};
    s.minimize();
    checkSequence(s, out2);
  }

  // |-------|
  //    |--|
  {
    vector<int64_t> in {10, 50, 20, 30};
    vector<int64_t> out {10, 20, 20, 30, 30, 50};
    initSequence(s, in);
    checkSequence(s, out);
    vector<int64_t> out2 {10, 50};
    s.minimize();
    checkSequence(s, out2);
  }

  // |-------|
  // |--|
  {
    vector<int64_t> in {10, 50, 10, 30};
    vector<int64_t> out {10, 30, 30, 50};
    initSequence(s, in);
    checkSequence(s, out);
  }

  // |-------|
  // |-------|
  {
    vector<int64_t> in {10, 50, 10, 50};
    vector<int64_t> out {10, 50};
    initSequence(s, in);
    checkSequence(s, out);
  }

  // |-------|
  // |------------|
  {
    vector<int64_t> in {10, 50, 10, 100};
    vector<int64_t> out {10, 50, 50, 100};
    initSequence(s, in);
    checkSequence(s, out);
  }

  // |-------|
  //     |------------|
  {
    vector<int64_t> in {10, 50, 20, 80};
    vector<int64_t> out {10, 20, 20, 50, 50, 80};
    initSequence(s, in);
    checkSequence(s, out);
    vector<int64_t> out2 {10, 80};
    s.minimize();
    checkSequence(s, out2);
  }

  // |-------|
  //             |------------|
  {
    vector<int64_t> in {10, 50, 80, 100};
    vector<int64_t> out {10, 50, 80, 100};
    initSequence(s, in);
    checkSequence(s, out);
    s.minimize();
    checkSequence(s, out);
  }
  
  //             |------------|
  // |-------|
  {
    vector<int64_t> in {80, 100, 10, 50};
    vector<int64_t> out {10, 50, 80, 100};
    initSequence(s, in);
    checkSequence(s, out);
    s.minimize();
    checkSequence(s, out);
  }
  
  //         |------------|
  // |-------|
  {
    vector<int64_t> in {50, 100, 10, 50};
    vector<int64_t> out {10, 50, 50, 100};
    initSequence(s, in);
    checkSequence(s, out);
    vector<int64_t> out2 {10, 100};
    s.minimize();
    checkSequence(s, out2);
  }
  
  // |-------|
  //         |------------|
  {
    vector<int64_t> in {10, 50, 50, 100};
    vector<int64_t> out {10, 50, 50, 100};
    initSequence(s, in);
    checkSequence(s, out);
    vector<int64_t> out2 {10, 100};
    s.minimize();
    checkSequence(s, out2);
  }

  //   |--|  |--|  |--|
  // |-------------------|
  {
    vector<int64_t> in {10, 20, 30, 40, 50, 60, 0, 70};
    vector<int64_t> out {0, 10, 10, 20, 20, 30, 30, 40, 40, 50, 50, 60, 60, 70};
    initSequence(s, in);
    checkSequence(s, out);
    s.minimize();
    vector<int64_t> out2 {0, 70};
    checkSequence(s, out2);
  }

  //   |---|   |---|   |---|
  // |---------------|
  {
    vector<int64_t> in {10, 30, 50, 70, 90, 110, -1, 80};
    vector<int64_t> out {-1, 10, 10, 30, 30, 50, 50, 70, 70, 80, 90, 110};
    initSequence(s, in);
    checkSequence(s, out);
    s.minimize();
    vector<int64_t> out2 {-1, 80, 90, 110};
    checkSequence(s, out2);
  }

  
  //   |ww|  |rr|  |ww|
  // |rrrrrrrrrrrrrrrrrr|
  //1|r|ww|rr|rr|rr|ww|r|
  //2|r|ww|rrrrrrrr|ww|r|
  {
    vector<int64_t> in {10, 20, Event::WRITE, 30, 40, Event::READ,
                        50, 60, Event::WRITE, 0, 70, Event::READ};
    vector<int64_t> out {0, 10, Event::READ, 10, 20, Event::WRITE,
                         20, 30, Event::READ, 30, 40, Event::READ,
                         40, 50, Event::READ, 50, 60, Event::WRITE,
                         60, 70, Event::READ};
    initSequence2(s, in);
    checkSequence2(s, out);
    s.minimize();
    vector<int64_t> out2 {0, 10, Event::READ, 10, 20, Event::WRITE,
                         20, 50, Event::READ, 50, 60, Event::WRITE,
                         60, 70, Event::READ};
    checkSequence2(s, out2);
  }

  cout << "OK\n";
}


RangeMerge::RangeMerge(File::RankSeqMap &rank_sequences) {
  // create vector of RankSeq objects
  for (auto &it : rank_sequences) {
    ranks.emplace_back(it.first, it.second.get());
  }

  // add all the RankSeq objects to a priority queue
  for (size_t i = 0; i < ranks.size(); i++)
    incoming_queue.push(ranks.data() + i);

  // initialize range to a junk value
  range_end = range_start = INT64_MIN;

  // initialize range_end to the beginning of the first incoming event,
  // so when next() is called, that will be the first value in range_start.
  if (!incoming_queue.empty()) {
    range_end = incoming_queue.top()->offset();
  }
}


bool RangeMerge::next() {
  if (incoming_queue.empty() && active_set.empty())
    return false;
  
  range_start = range_end;

  // expire all the events that are ending
  while (!outgoing_queue.empty() &&
         outgoing_queue.top()->endOffset() == range_start) {
    RankSeq *rs = outgoing_queue.top();
    outgoing_queue.pop();
    active_set.erase(rs->rank());
    
    // if this rank has more events, push it back into incoming_queue
    if (rs->next())
      incoming_queue.push(rs);
  }

  // all done?
  if (incoming_queue.empty() && active_set.empty())
    return false;
  
  // start all events that are starting
  while (!incoming_queue.empty() &&
         incoming_queue.top()->offset() == range_start) {
    RankSeq *rs = incoming_queue.top();
    incoming_queue.pop();

    // as it's on the incoming queue, this RankSeq should not be done
    assert(!rs->done());
    
    // this rank should not be currently active
    assert(active_set.find(rs->rank()) == active_set.end());

    active_set[rs->rank()] = rs->event().mode;
    outgoing_queue.push(rs);
  }

  // find the end of this subrange, which is when the next event expires
  // or the next one starts, whichever comes first.
  assert(!incoming_queue.empty() || !outgoing_queue.empty());
  if (incoming_queue.empty()) {
    range_end = outgoing_queue.top()->endOffset();
  } else if (outgoing_queue.empty()) {
    range_end = incoming_queue.top()->offset();
  } else {
    range_end = min(outgoing_queue.top()->endOffset(),
                    incoming_queue.top()->offset());
  }

  return true;
}

