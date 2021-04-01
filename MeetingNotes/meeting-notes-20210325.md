# Mar 25th, 2020

### SC21 Paper Discussion

- System Prototype, Lei
  - IOR, mdtest
  - Apps, specfem, wrf, namd, ResNet-50 (problem)

- Benchmark DAOS, Ian
  - 1, 2, 4 servers are working fine
  - IOR, 10% diff between native/POSIX interface
  - mdtest -- slow, as interception impl only supports read/write, all metadata operations go through fuse.
  - Apps, specfem (I/O 10x slower than Lustre), ResNet-50 (fuse), run ResNet with sync I/O on DAOS.

- Experiment Design, 
  - Run on Lustre
