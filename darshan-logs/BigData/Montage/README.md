Please record the CMD, number of nodes, and configuration for each run along with the Darshan log.

# Execution
The logs here are generated from the example of Montage Tutorial [here](http://montage.ipac.caltech.edu/docs/m101tutorial.html).
In particular, below are the commands executed after downloading the [tutorial-initial.tar.gz](http://montage.ipac.caltech.edu/docs/m101Example/tutorial-initial.tar.gz) file.
```
tar -xf tutorial-initial.tar.gz
cd m101/
export STRACE_RUN="strace -y -o montage.log -A"
eval "$STRACE_RUN" mImgtbl rawdir images-rawdir.tbl
eval "$STRACE_RUN" mProjExec -p rawdir images-rawdir.tbl template.hdr projdir stats.tbl
eval "$STRACE_RUN" mImgtbl projdir images.tbl
eval "$STRACE_RUN" mAdd -p projdir images.tbl template.hdr final/m101_uncorrected.fitseval "$STRACE_RUN" ../Montage-6.0/bin/mJPEG -gray final/m101_uncorrected.fits 20% 99.98% loglog -out final/m101_uncorrected.jpg
eval "$STRACE_RUN" mOverlaps images.tbl diffs.tbl
eval "$STRACE_RUN" mDiffExec -p projdir diffs.tbl template.hdr diffdir
eval "$STRACE_RUN" mFitExec diffs.tbl fits.tbl diffdir
eval "$STRACE_RUN" mBgModel images.tbl fits.tbl corrections.tbl
eval "$STRACE_RUN" mBgExec -p projdir images.tbl corrections.tbl corrdir
eval "$STRACE_RUN" mAdd -p corrdir images.tbl template.hdr final/m101_mosaic.fits
eval "$STRACE_RUN" mJPEG -gray final/m101_mosaic.fits 0s max gaussian-log -out final/m101_mosaic.jpg
```
Note that we use strace to capture the I/O calls and combine the output of all processing steps to the `montage.log` file. 
The `-y` argument is needed so that strace will show the filename instead of the file handle in each individual read or write call.

# Analysis
The raw strace output also contains I/O to files or libraries under the system path, which is not our focus here.
So we use the [Stana](https://github.com/johnlcf/Stana/wiki) tool and run `strace_analyser -e StatFileIO montage.log` to get a summary of all the files in the `montage.log`, and then removed the entries that start with a `/`.
After that, we got the list of files of interest in `file_list.txt` that can be used to drive the nest step of parsing the I/O logs.
This is done with the following loop:
```
for i in `awk 'NR>1 {print($1);}' file_list.txt`
do
  echo "filename: ${i}"
  cat montage.log | grep $i | awk '{print substr($0,1,10)}' | uniq -c
done
```
and its output is in `summary.txt`.
This loop grabs the I/O calls applied to a file in that list, and then combines and counts the consecutive reads or writes in the outputs. 
The output basically shows the order of reads and writes applied to each file within the job.
The next step will be analysing the read/write dependency based on that order.
