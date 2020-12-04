Please record the CMD, number of nodes, and configuration for each run along with the Darshan log.


Using analysis/darshan_file_access_summary.py:

        darshan-parser aroraish_wrf.exe_id2135019_12-3-45527-5597752660095964326_5076852.darshan | \
          ../../analysis/darshan_file_access_summary.py

No file access conflicts detected for this application.

It accesses 239 files:

          1 MULTIPLE_RO - namelist.input is read by all ranks
          2 MULTIPLE_WO - STDOUT and STDERR are written to by all ranks
          3 SINGLE_RO - 3 files are read by a single rank
          9 SINGLE_RW - 9 files are read and written by a single rank
        224 SINGLE_WO - 224 files are written to by a single rank

