# Benchmark Description
This is a tiny benchmark of a simulation over 1x64x64 grid and 1 minutie of seismic record. 
ADIOS running on top of MPI-IO is used in this run. 
Note that the Specfem3D simulation runs with two steps, the first step is called `xmeshfem3D`, which generates the cube-sphere mesh for the simulation.
Then, the `xspecfem3D` runs the actual simulation with that mesh. 
In a typical seismic research, the `xmeshfem3D` usually runs once, but the `xspecfem3D` runs multiple times especially in a inversion workflow.
Here, we recorded the darshan logs for both application, but it is not necessarily required to analyse the IO dependency between `xmeshfem3D` and `xspecfem3D`.

### Analysis

Using  `darshan_dxt_conflicts` to scan for conflicts, where a conflict is defined as a range of bytes that is read or written by more than one process, and at least one of those acesses is a write.

        ./darshan_dxt_conflicts ../HPC/specfem3D/xmeshfem3D_dxt.log
        
Three files have write-after-write conflicts, and they all have a ".bp" suffix. In each case, both rank 0 and some other rank write to a subrange of the file.  The subranges vary form 10 bytes to 21856 bytes.

        /scratch1/06058/iwang/benchmarks/specfem3d/tinytest_rtx_8_adios_darshan/DATABASES_MPI/boundary.bp
          CONFLICT bytes 7391612..7413468: writers=0,1
          CONFLICT bytes 7413469..7435325: writers=0,2
          CONFLICT bytes 7435326..7448165: writers=0,3
        
        /scratch1/06058/iwang/benchmarks/specfem3d/tinytest_rtx_8_adios_darshan/DATABASES_MPI/solver_data_mpi.bp
          CONFLICT bytes 1441851..1445354: writers=0,1
          CONFLICT bytes 1445355..1450146: writers=0,2
          CONFLICT bytes 1450147..1454938: writers=0,3
          CONFLICT bytes 1454939..1459730: writers=0,4
          CONFLICT bytes 1459731..1459741: writers=0,5
        
        /scratch1/06058/iwang/benchmarks/specfem3d/tinytest_rtx_8_adios_darshan/DATABASES_MPI/stacey.bp
          CONFLICT bytes 149413..154606: writers=0,1
          CONFLICT bytes 154607..156013: writers=0,2
          CONFLICT bytes 187653..189534: writers=0,1
          CONFLICT bytes 189535..191416: writers=0,2
          CONFLICT bytes 191417..193298: writers=0,3
          CONFLICT bytes 193299..195180: writers=0,4
          CONFLICT bytes 195181..197062: writers=0,5
          CONFLICT bytes 197063..198944: writers=0,6
          CONFLICT bytes 198945..200826: writers=0,7


TODO: detect false sharing, where two writers modify non-overlapping data in the same block.
