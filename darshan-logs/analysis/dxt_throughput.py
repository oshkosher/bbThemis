#!/usr/bin/env python3

import sys, traceback, csv, functools

"""
strace_to_dxt file format
  
First line: "# strace io log"
Remaining lines are tab-delimited.

<pid> open|openat <fd> <file_name>
<pid> open|openat <fd> <file_name> 1   # for files opened with O_APPEND
0     1           2    3           4

<pid> read|pread64|write <offset> <length> <timestamp> <fd>
0     1                  2        3        4           5
"""

DEFAULT_BIN_COUNT = 30

HELP_STR = f"""
  dxt_throughput.py [opt]
    Read from stdin either Darshan DXT or the output of strace_to_dxt.py.
    Output the IO throughput over time in the following tab-delimited format:
    <timestamp> <read_bytes> <read_bps> <read_count> <reads/sec> <write_bytes> <write_bps> <write_count> <writes/sec>
      [bps=bytes per second]
    Data is grouped into fixed-size time spans, like a histogram.
  opt:
    -h : print help
    -bins <bin_count> : group time slices into this number of bins.
       default = {DEFAULT_BIN_COUNT}
"""

def main(args):

  opt = parseArgs(args)
  if not opt: return 1

  bin_count = opt['bin_count']
  
  # [(timestamp, byte_count), ...]
  reads = []
  writes = []

  readFile(sys.stdin, reads, writes)

  # print(f'{len(reads)} reads')
  # for i, r in enumerate(reads):
  #   print(f'  {i:5d} {r[0]:8.6f} {r[1]:8d}')

  # print(f'{len(writes)} writes')
  # for i, r in enumerate(writes):
  #   print(f'  {i:5d} {r[0]:8.6f} {r[1]:8d}')

  reads.sort()
  writes.sort()

  if len(reads) == 0:
    if len(writes) == 0:
      sys.stderr.write('No reads or writes.')
      return 0
    start_time = writes[0][0]
    end_time = writes[-1][0]
  elif len(writes) == 0:
    start_time = reads[0][0]
    end_time = reads[-1][0]
  else:
    start_time = min(writes[0][0], reads[0][0])
    end_time = min(writes[-1][0], reads[-1][0])

  if end_time <= start_time:
    sys.stderr.write(f'Invalid time range. start_time={start_time} end_time={end_time}')
    return 1

  bin_size = (end_time - start_time) / bin_count

  def sumBytes(io_list):
    return functools.reduce(lambda a,b: a+b, [x[1] for x in io_list])

  sys.stderr.write(f'{len(reads)} reads {commafy(sumBytes(reads))} bytes, {len(writes)} writes {commafy(sumBytes(writes))} bytes\n')
  sys.stderr.write(f'timestamps {start_time} .. {end_time}, {end_time-start_time:.3f} sec elapsed.\n')
  sys.stderr.write(f'bin size = {bin_size:.8f} sec\n')

  reads_pos = 0
  writes_pos = 0
  # bins = []

  print('# bin_start_time\tread_bytes\tread_bytes_per_sec\tread_count\treads_per_sec\twrite_bytes\twrite_bytes_per_sec\twrite_count\twrites_per_sec')
  
  for bin_no in range(bin_count):
    bin_start_time = start_time + bin_size * bin_no
    bin_end_time = end_time if bin_no == bin_count-1 else start_time + bin_size * (bin_no+1)
    # print(f'bin_no {bin_no} {bin_start_time} .. {bin_end_time}')
    (read_bytes, reads_next_pos) = sumIO(reads, reads_pos, bin_end_time)
    (write_bytes, writes_next_pos) = sumIO(writes, writes_pos, bin_end_time)
    # bins.append( (bin_start_time, read_bytes, write_bytes) )

    time_midpoint = bin_start_time - start_time + bin_size/2
    read_count = reads_next_pos - reads_pos
    reads_pos = reads_next_pos
    write_count = writes_next_pos - writes_pos
    writes_pos = writes_next_pos

    print(f'{time_midpoint:.6f}\t{read_bytes}\t{read_bytes/bin_size:.0f}\t{read_count}\t{read_count/bin_size:.2f}\t{write_bytes}\t{write_bytes/bin_size:.0f}\t{write_count}\t{write_count/bin_size:.2f}')


def commafy(n):
  n = int(n)
  start = 2 if n < 0 else 1
  s = str(n)
  p = len(s) - 3
  while p >= start:
    s = s[:p] + ',' + s[p:]
    p -= 3
  return s


def sumIO(io_list, pos, end_time):
  """
  Given io_list = [(timestamp, byte_count), ...], sorted by timestamp
  pos is an index in io_list
  end_time is either a timestamp or None

  Find end_index, where io_list[end_index] is the index of the first entry in
  io_list such that timestamp > end_time.
  Sum the byte_count values in io_list from [pos..end_index).
  Return (sum_byte_count, end_index).
  """
  sum_byte_count = 0
  # print(f'sum to {end_time}')
  while pos < len(io_list) and io_list[pos][0] <= end_time:
    # print(f'  {pos}')
    sum_byte_count += io_list[pos][1]
    pos += 1

  return (sum_byte_count, pos)
    

def parseArgs(args):
  opt = {'bin_count': DEFAULT_BIN_COUNT}
  
  argno = 0
  while argno < len(args) and len(args[argno])>0 and args[argno][0]=='-':
    arg = args[argno]
    
    if arg == '-h':
      printHelp()

    elif arg == '-bins':
      argno += 1
      arg = args[argno]
      try:
        opt['bin_count'] = int(arg)
      except ValueError:
        opt['bin_count'] = -1
      if opt['bin_count'] < 1:
        sys.stderr.write('Invalid bin count: ' + arg)
        return None

    argno += 1

  return opt

      
def printHelp():
  sys.stderr.write(HELP_STR)
  sys.exit(0)

      
def readFile(inf, reads, writes):

  header = inf.readline()
  if header.startswith('# strace io log'):
    readStraceIOLog(inf, reads, writes)
  elif header.startswith('# darshan log'):
    readDarshanDxtLog(inf, reads, writes)
  else:
    sys.stderr.write('Unrecognized input file format.')
    sys.exit(1)
  
    
def readStraceIOLog(inf, reads, writes):
  # (pid,fd) -> filename
  open_files = {}

  csv_reader = csv.reader(sys.stdin, delimiter='\t', strict=True)
  for row in csv_reader:
    if row[0].startswith('# strace io log'):
      continue
    
    try:
      pid = int(row[0])
      fn_name = row[1]
      
      if fn_name.startswith('open'):
        fd = int(row[2])
        filename = row[3]
        open_files[(pid,fd)] = filename
        
      elif fn_name in ('read', 'pread64', 'write'):
        fd = int(row[5])
        if (pid,fd) in open_files:
          timestamp = float(row[4])
          byte_count = int(row[3])
          dest = writes if fn_name=='write' else reads
          dest.append((timestamp, byte_count))
        else:
          # print('ignore ' + repr(row))
          pass
        
    except Exception as e:
      print(f'Error on line {csv_reader.line_num}: {e}')
      traceback.print_tb(sys.exc_info()[2])


def readDarshanDxtLog(inf, reads, writes):
  sys.stderr.write('DXT parsing not implemented yet')
  sys.exit(1)
    


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
  
