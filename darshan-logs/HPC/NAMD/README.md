Please record the CMD, number of nodes, and configuration for each run along with the Darshan log.

### Analysis

Using  `analysis/darshan_file_access_summary.py`, which parses the output of `darshan-parser`

        darshan-parser aroraish_namd2_id2135028_12-3-45742-11770750271981201520_12230946.darshan | \
          ../../analysis/darshan_file_access_summary.py

No file access conflicts detected for this application.

It accesses 12 files. Multiple ranks write to stdout, but all other I/O
is done only by rank 0.

        1 MULTIPLE_WO - multiple ranks write to stdout
        4 SINGLE_RO - 4 files read by rank 0
        1 SINGLE_RW - 1 file read from and written to by rank 0
        6 SINGLE_WO - 6 files written by rank 0
