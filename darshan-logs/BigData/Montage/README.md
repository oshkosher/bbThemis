Please record the CMD, number of nodes, and configuration for each run along with the Darshan log.

# Execution
The logs here are generated from the example of Montage Tutorial [here](http://montage.ipac.caltech.edu/docs/m101tutorial.html).
In particular, below are the commands executed after downloading the [tutorial-initial.tar.gz](http://montage.ipac.caltech.edu/docs/m101Example/tutorial-initial.tar.gz) file.
```
tar -xf tutorial-initial.tar.gz
cd m101/
export STRACE_RUN="strace -ttt -f -y -o trace.txt -A"
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

The default Makefile of Montage does not run the "Exec" commands with parallelism, so we have manually specified the parallel versions of them to be compiled. Also, there is an [issue](https://github.com/Caltech-IPAC/Montage/issues/25) of `tsave` and `trestore` missing in the mtbl library. We fixed it by copying those missing functions from Montage 4.1.
