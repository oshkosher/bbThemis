#!/usr/bin/env python3

# File access conflict analysis tool, developed as part of
# the bbThemis project (https://github.com/bbThemis/bbThemis)
#
# This reads the output of darshan-parser (which decodes Darshan log files)
# and tracks which processes (aka ranks) read from or wrote to each file
# accessed by the application.
#
# Note: this does not yet do a detailed analysis of the per-call data.
# If two ranks write to different byte ranges of one file, this tool
# will think its a conflict and label the accesses MULTIPLE_WO.
#
# Ed Karrels, edk@illinois.edu, December 2020


import sys

help_str = """
  darshan-parser <logfile> | darshan_file_access_summary.py

  This uses the POSIX_BYTES_READ and POSIX_BYTES_WRITTEN fields in the
  Darshan data to generate a list of all the files accessed during the
  run and summarize how many ranks read from or write to each file.

  SINGLE_{RO,WO,RW} - a single rank accesses the file as read-only, write-only,
    or read-write (respectively)
  MULTIPLE_{RO,WO,RW} - multiple ranks access the file
  NONE - no ranks read from or write to the file

  Conflicts are only possible with MULTIPLE_WO or MULTIPLE_RW.

  TODOs
   - use DxT (per-call) log data to check timestamps and file ranges for
     conflicts
   - check data from the STDIO routines (fread/fwrite) and MPIIO routines
     (e.g. MPI_File_write) for access conflicts
"""

VERBOSE = False

# enumeration of field indices
FIELD_MODULE = 0
FIELD_RANK = 1
FIELD_FILE_NAME_HASH = 2
FIELD_COUNTER_NAME = 3
FIELD_COUNTER_VALUE = 4
FIELD_FILE_NAME = 5
FIELD_MOUNT_POINT = 6
FIELD_FILE_SYSTEM_TYPE = 7
FIELD_COUNT = 8


def main(args):
  if len(args) > 0: printHelp();

  # file_hash -> FileInfo
  files = readDarshanOutput()

  print('accesses\tfilename\trank_detail')

  # FileInfo objects
  for f in files.values():
    print(f'{f.accessSummary()}\t{escapeString(f.filename)}\t{f.accessDetail()}')


def escapeString(s):
  return s.replace('\n', '\\n').replace('\t', '\\t')


def rankSetCount(s):
  # given a set of ranks, returns:
  # 0 : if the set is empty
  # 1 : if the set contains a single rank (and it is not -1)
  # 2 : if the set contains more than one rank or -1
  if -1 in s or len(s) > 1:
    return 2
  elif len(s) == 0:
    return 0
  else:
    return 1


def rankSetList(s):
  """
  Returns a string listing all the ranks in a set. Empty set: "none".
  Set containing -1: "all".
  """
  if len(s) == 0:
    return 'none'
  elif -1 in s:
    return 'all'
  else:
    return ','.join([str(x) for x in s])


def rankCount(s):
  if -1 in s:
    return 'all'
  else:
    return len(s)


def readDarshanOutput():

  # file_hash -> FileInfo
  files = {}
  current_file = None
  current_file_hash = None
  line_no = 0

  for line in sys.stdin:
    line_no += 1

    # ignore anything that isn't part of the POSIX module
    if not line.startswith('POSIX\t'): continue

    # each line should have 8 fields: <module>, <rank>, <record id>, <counter>, <value>, <file name>, <mount pt>, <fs type>
    fields = line.split('\t')
    if len(fields) != FIELD_COUNT:
      print(f'Error line {line_no}: {len(fields)} fields (expected {FIELD_COUNT})')
      continue

    # ignore everything but # of bytes read or written
    counter_name = fields[FIELD_COUNTER_NAME]
    if counter_name not in ['POSIX_BYTES_READ', 'POSIX_BYTES_WRITTEN']:
      continue

    # ignore if no bytes read or written
    byte_count = int(fields[FIELD_COUNTER_VALUE])
    if byte_count <= 0:
      continue
    
    file_hash = int(fields[FIELD_FILE_NAME_HASH])
    if file_hash != current_file_hash:
      if file_hash not in files:
        file_name = fields[FIELD_FILE_NAME]
        current_file = FileInfo(file_name)
        files[file_hash] = current_file
      else:
        current_file = files[file_hash]

    rank = int(fields[FIELD_RANK])

    if counter_name == 'POSIX_BYTES_READ':
      current_file.addReader(rank)
      if VERBOSE:
        print(f'{current_file.filename}: {rank} read {byte_count} bytes')
    else:
      assert(counter_name == 'POSIX_BYTES_WRITTEN')
      current_file.addWriter(rank)
      if VERBOSE:
        print(f'{current_file.filename}: {rank} wrote {byte_count} bytes')

  return files


class FileInfo:
  """Store per-file data"""

  def __init__(self, filename):
    self.filename = filename
    self.readers = set()
    self.writers = set()

  def addReader(self, rank):
    self.readers.add(rank)

  def addWriter(self, rank):
    self.writers.add(rank)

  def accessSummary(self):
    nw = rankSetCount(self.writers)  # number of writers 0, 1, or 2
    nr = rankSetCount(self.readers)  # number of readers 0, 1, or 2

    if nw == 0:
      if nr == 0:
        return 'NONE'
      elif nr == 1:
        return 'SINGLE_RO'
      else:
        return 'MULTIPLE_RO'
    elif nw == 1:
      if nr == 0:
        return 'SINGLE_WO'
      elif nr == 1:
        assert(len(self.readers) == 1 and len(self.writers) == 1
               and -1 not in self.readers and -1 not in self.writers)
        if self.readers == self.writers:
          return 'SINGLE_RW'
        else:
          return 'MULTIPLE_RW'
      else:
        return 'MULTIPLE_RW'
    else:
      # multiple writers
      if nr == 0:
        return 'MULTIPLE_WO'
      else:
        return 'MULTIPLE_RW'

  def accessDetail(self):
    return f'readers={rankSetList(self.readers)},writers={rankSetList(self.writers)}'
      
def printHelp():
  print(help_str)
  sys.exit(1)
    


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
  
