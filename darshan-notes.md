

# Building Darshan

Pre-built installation on Frontera: `/home1/07811/edk/darshan-3.2.1`

Or to build your own copy:

1. Download from [Argonne National Laboratory](https://www.mcs.anl.gov/research/projects/darshan/)

        wget ftp://ftp.mcs.anl.gov/pub/darshan/releases/darshan-3.2.1.tar.gz

1. Decompress the archive.

        tar zxf darshan-3.2.1.tar.gz

1. Configure/build/install darshan-runtime (the part that generates logs).
**Be sure to disable Lustre support** - there is currently a bug in
that causes failures during initialization if the Lustre module is enabled.
[Details on that](https://xgitlab.cels.anl.gov/darshan/darshan/-/issues/270).

        cd darshan-3.2.1/darshan-runtime
        ./configure --prefix=$HOME/darshan-3.2.1 \
          --with-log-path=$HOME/darshan_logs \
          --with-jobid-env=SLURM_JOBID \
          --with-log-path-by-env=DARSHAN_LOGPATH \
          --enable-hdf5-mod=/opt/apps/intel19/hdf5/1.10.4/x86_64/lib \
          --enable-pnetcdf-mod --disable-lustre-mod
        make
        make install

1. Configure/build/install darshan-util (the part that analyses logs).

        cd ../darshan-util
        ./configure --prefix=$HOME/darshan-3.2.1
        make
        make install

# Generating logs

Set the LD_PRELOAD environment variable to the location of `libdarshan.so`.
It is not necessary to recompile code. 

    export LD_PRELOAD=/home1/07811/edk/darshan-3.2.1/lib/libdarshan.so
    ibrun -n 4 ./test_darshan_logging
    
Note that the logfile is generated at the end of the run during the call
to MPI_Finalize. So if job doesn't call MPI_Finalize or crashes before MPI_Finalize completes,
a logfile will not be generated.

### Enabling per-call logging

By default Darshan only logs summary statistics, such as the number of calls to `write()` 
and the total time spent in those calls. For more detailed statistics on each
call (rank, offset, length, start time, end time), 
enable the "Darshan Extended Tracing" (DxT) feature by setting the `DXT_ENABLE_IO_TRACE`
environment variable.

    export DXT_ENABLE_IO_TRACE=1
    export LD_PRELOAD=/home1/07811/edk/darshan-3.2.1/lib/libdarshan.so
    ibrun -n 4 ./test_darshan_logging

### Destination directory

The logfile will be saved in a subdirectory tree that groups files by calendar day.  
The filename will include your username, the name of the executable, and the job id.
For example: `/home1/07811/edk/darshan_logs/2020/11/27/edk_test_darshan_logging_id2106773_11-27-68391-7414901189305593357_11735009.darshan`

The default logfile location is `/home1/07811/edk/darshan_logs`
(this was compiled-in via the `configure` command above). To override this, set
the environment variable DARSHAN_LOGPATH to the desired directory.

    export LD_PRELOAD=/home1/07811/edk/darshan-3.2.1/lib/libdarshan.so
    export DARSHAN_LOGPATH=/home1/07811/edk/tmp
    ibrun -n 4 ./test_darshan_logging
    
*/home1/07811/edk/darshan_logs is currently world-writable so anyone can create logfiles in that directory
structure, but this also means a malicious user could mess with other users' logfiles. It is recommended that you
always set DARSHAN_LOGPATH to one of your own directories.*

# Analyzing logs

See the [documentation on the Darshan project page](https://www.mcs.anl.gov/research/projects/darshan/docs/darshan-util.html).

Here are a few of the tools (available in `/home1/07811/edk/darshan-3.2.1/bin`) which can be used to
analyze the data in a Darshan logfile.

* `darshan-parser` - Extract all the data in the log in human-readable tab-delimited format.
* `darshan-dxt-parser` - Extract per-call details, if DxT tracing was enabled.
* `darshan-job-summary.pl` - Generate a PDF graphical summary of the I/O activity for a job. (requires pdflatex)
* `darshan-summary-per-file.sh` - Similar to darshan-job-summary.pl, but generates a separate PDF for each file accessed by the application.
