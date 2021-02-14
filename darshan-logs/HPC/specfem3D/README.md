# Benchmark Description
This is a tiny benchmark of a simulation over 1x64x64 grid and 1 minutie of seismic record. 
ADIOS running on top of MPI-IO is used in this run. 
Note that the Specfem3D simulation runs with two steps, the first step is called `xmeshfem3D`, which generates the cube-sphere mesh for the simulation.
Then, the `xspecfem3D` runs the actual simulation with that mesh. 
In a typical seismic research, the `xmeshfem3D` usually runs once, but the `xspecfem3D` runs multiple times especially in a inversion workflow.
Here, we recorded the darshan logs for both application, but it is not necessarily required to analyse the IO dependency between `xmeshfem3D` and `xspecfem3D`.

### Analysis

Using  `darshan_dxt_conflicts` to scan for conflicts, where a conflict is defined as a range of bytes that is read or written by more than one process, and at least one of those acesses is a write.

        (cd ../../analysis; make)
        ../../analysis/darshan_dxt_conflicts -audit xmeshfem3D_dxt.log
        
Three files have write-after-write conflicts, and they all have a ".bp" suffix. In each case, both rank 0 and some other rank write to a subrange of the file.  The subranges vary from 10 bytes to 21856 bytes.

        /scratch1/06058/iwang/benchmarks/specfem3d/tinytest_rtx_8_adios_darshan/DATABASES_MPI/boundary.bp
          CONFLICT bytes 7391612..7413468: writers=0,1
          time 34.7684-34.7701 rank 0 MPI-IO write bytes 7369755..7448165 (conflict overlap 21857 bytes)
          time 34.7685-34.7700 rank 0 POSIX  write bytes 7369755..7448165 (conflict overlap 21857 bytes)
          time 34.8251-34.8253 rank 0 MPI-IO read  bytes 7370603..7447752 (conflict overlap 21857 bytes)
          time 34.8252-34.8252 rank 0 POSIX  read  bytes 7370603..7447752 (conflict overlap 21857 bytes)
          time 34.8267-34.8270 rank 1 MPI-IO write bytes 7391612..7413468 (conflict overlap 21857 bytes)
          time 34.8268-34.8269 rank 1 POSIX  write bytes 7391612..7413468 (conflict overlap 21857 bytes)
          CONFLICT bytes 7413469..7435325: write ranks={2} read/write ranks={0}
          time 34.7684-34.7701 rank 0 MPI-IO write bytes 7369755..7448165 (conflict overlap 21857 bytes)
          time 34.7685-34.7700 rank 0 POSIX  write bytes 7369755..7448165 (conflict overlap 21857 bytes)
          time 34.8251-34.8253 rank 0 MPI-IO read  bytes 7370603..7447752 (conflict overlap 21857 bytes)
          time 34.8252-34.8252 rank 0 POSIX  read  bytes 7370603..7447752 (conflict overlap 21857 bytes)
          time 34.8267-34.8270 rank 2 MPI-IO write bytes 7413469..7435325 (conflict overlap 21857 bytes)
          time 34.8268-34.8269 rank 2 POSIX  write bytes 7413469..7435325 (conflict overlap 21857 bytes)
          CONFLICT bytes 7435326..7448165: write ranks={3} read/write ranks={0}
          time 34.7684-34.7701 rank 0 MPI-IO write bytes 7369755..7448165 (conflict overlap 12840 bytes)
          time 34.7685-34.7700 rank 0 POSIX  write bytes 7369755..7448165 (conflict overlap 12840 bytes)
          time 34.8249-34.8250 rank 0 MPI-IO read  bytes 7448138..7448165 (conflict overlap 28 bytes)
          time 34.8250-34.8250 rank 0 POSIX  read  bytes 7448138..7448165 (conflict overlap 28 bytes)
          time 34.8251-34.8253 rank 0 MPI-IO read  bytes 7370603..7447752 (conflict overlap 12427 bytes)
          time 34.8252-34.8252 rank 0 POSIX  read  bytes 7370603..7447752 (conflict overlap 12427 bytes)
          time 34.8256-34.8257 rank 0 MPI-IO read  bytes 7447753..7448137 (conflict overlap 385 bytes)
          time 34.8256-34.8256 rank 0 POSIX  read  bytes 7447753..7448137 (conflict overlap 385 bytes)
          time 34.8267-34.8269 rank 3 MPI-IO write bytes 7435326..7457182 (conflict overlap 12840 bytes)
          time 34.8268-34.8268 rank 3 POSIX  write bytes 7435326..7457182 (conflict overlap 12840 bytes)
        
        /scratch1/06058/iwang/benchmarks/specfem3d/tinytest_rtx_8_adios_darshan/DATABASES_MPI/solver_data_mpi.bp
          CONFLICT bytes 1441851..1445354: write ranks={1} read/write ranks={0}
          time 34.7977-34.7979 rank 0 MPI-IO write bytes 1438347..1459741 (conflict overlap 3504 bytes)
          time 34.7978-34.7978 rank 0 POSIX  write bytes 1438347..1459741 (conflict overlap 3504 bytes)
          time 34.8382-34.8383 rank 0 MPI-IO read  bytes 1439227..1459322 (conflict overlap 3504 bytes)
          time 34.8383-34.8383 rank 0 POSIX  read  bytes 1439227..1459322 (conflict overlap 3504 bytes)
          time 34.8394-34.8402 rank 1 MPI-IO write bytes 1441851..1445354 (conflict overlap 3504 bytes)
          time 34.8395-34.8401 rank 1 POSIX  write bytes 1441851..1445354 (conflict overlap 3504 bytes)
          CONFLICT bytes 1445355..1450146: write ranks={2} read/write ranks={0}
          time 34.7977-34.7979 rank 0 MPI-IO write bytes 1438347..1459741 (conflict overlap 4792 bytes)
          time 34.7978-34.7978 rank 0 POSIX  write bytes 1438347..1459741 (conflict overlap 4792 bytes)
          time 34.8382-34.8383 rank 0 MPI-IO read  bytes 1439227..1459322 (conflict overlap 4792 bytes)
          time 34.8383-34.8383 rank 0 POSIX  read  bytes 1439227..1459322 (conflict overlap 4792 bytes)
          time 34.8394-34.8417 rank 2 MPI-IO write bytes 1445355..1450146 (conflict overlap 4792 bytes)
          time 34.8395-34.8416 rank 2 POSIX  write bytes 1445355..1450146 (conflict overlap 4792 bytes)
          CONFLICT bytes 1450147..1454938: write ranks={3} read/write ranks={0}
          time 34.7977-34.7979 rank 0 MPI-IO write bytes 1438347..1459741 (conflict overlap 4792 bytes)
          time 34.7978-34.7978 rank 0 POSIX  write bytes 1438347..1459741 (conflict overlap 4792 bytes)
          time 34.8382-34.8383 rank 0 MPI-IO read  bytes 1439227..1459322 (conflict overlap 4792 bytes)
          time 34.8383-34.8383 rank 0 POSIX  read  bytes 1439227..1459322 (conflict overlap 4792 bytes)
          time 34.8394-34.8405 rank 3 MPI-IO write bytes 1450147..1454938 (conflict overlap 4792 bytes)
          time 34.8395-34.8404 rank 3 POSIX  write bytes 1450147..1454938 (conflict overlap 4792 bytes)
          CONFLICT bytes 1454939..1459730: write ranks={4} read/write ranks={0}
          time 34.7977-34.7979 rank 0 MPI-IO write bytes 1438347..1459741 (conflict overlap 4792 bytes)
          time 34.7978-34.7978 rank 0 POSIX  write bytes 1438347..1459741 (conflict overlap 4792 bytes)
          time 34.8364-34.8377 rank 0 MPI-IO read  bytes 1459714..1459741 (conflict overlap 17 bytes)
          time 34.8365-34.8376 rank 0 POSIX  read  bytes 1459714..1459741 (conflict overlap 17 bytes)
          time 34.8382-34.8383 rank 0 MPI-IO read  bytes 1439227..1459322 (conflict overlap 4384 bytes)
          time 34.8383-34.8383 rank 0 POSIX  read  bytes 1439227..1459322 (conflict overlap 4384 bytes)
          time 34.8384-34.8386 rank 0 MPI-IO read  bytes 1459323..1459713 (conflict overlap 391 bytes)
          time 34.8385-34.8385 rank 0 POSIX  read  bytes 1459323..1459713 (conflict overlap 391 bytes)
          time 34.8395-34.8416 rank 4 POSIX  write bytes 1454939..1459730 (conflict overlap 4792 bytes)
          time 34.8395-34.8417 rank 4 MPI-IO write bytes 1454939..1459730 (conflict overlap 4792 bytes)
          CONFLICT bytes 1459731..1459741: write ranks={5} read/write ranks={0}
          time 34.7977-34.7979 rank 0 MPI-IO write bytes 1438347..1459741 (conflict overlap 11 bytes)
          time 34.7978-34.7978 rank 0 POSIX  write bytes 1438347..1459741 (conflict overlap 11 bytes)
          time 34.8364-34.8377 rank 0 MPI-IO read  bytes 1459714..1459741 (conflict overlap 11 bytes)
          time 34.8365-34.8376 rank 0 POSIX  read  bytes 1459714..1459741 (conflict overlap 11 bytes)
          time 34.8393-34.8404 rank 5 MPI-IO write bytes 1459731..1464522 (conflict overlap 11 bytes)
          time 34.8394-34.8403 rank 5 POSIX  write bytes 1459731..1464522 (conflict overlap 11 bytes)
        
        /scratch1/06058/iwang/benchmarks/specfem3d/tinytest_rtx_8_adios_darshan/DATABASES_MPI/stacey.bp
          CONFLICT bytes 149413..154606: write ranks={1} read/write ranks={0}
          time 18.5033-18.5038 rank 0 MPI-IO write bytes 144219..156013 (conflict overlap 5194 bytes)
          time 18.5034-18.5037 rank 0 POSIX  write bytes 144219..156013 (conflict overlap 5194 bytes)
          time 34.5865-34.5866 rank 0 MPI-IO read  bytes 144635..155606 (conflict overlap 5194 bytes)
          time 34.5865-34.5865 rank 0 POSIX  read  bytes 144635..155606 (conflict overlap 5194 bytes)
          time 34.5878-34.5879 rank 1 POSIX  write bytes 149413..154606 (conflict overlap 5194 bytes)
          time 34.5878-34.5879 rank 1 MPI-IO write bytes 149413..154606 (conflict overlap 5194 bytes)
          CONFLICT bytes 154607..156013: write ranks={2} read/write ranks={0}
          time 18.5033-18.5038 rank 0 MPI-IO write bytes 144219..156013 (conflict overlap 1407 bytes)
          time 18.5034-18.5037 rank 0 POSIX  write bytes 144219..156013 (conflict overlap 1407 bytes)
          time 34.5862-34.5862 rank 0 POSIX  read  bytes 155986..156013 (conflict overlap 28 bytes)
          time 34.5862-34.5863 rank 0 MPI-IO read  bytes 155986..156013 (conflict overlap 28 bytes)
          time 34.5865-34.5866 rank 0 MPI-IO read  bytes 144635..155606 (conflict overlap 1000 bytes)
          time 34.5865-34.5865 rank 0 POSIX  read  bytes 144635..155606 (conflict overlap 1000 bytes)
          time 34.5866-34.5867 rank 0 MPI-IO read  bytes 155607..155985 (conflict overlap 379 bytes)
          time 34.5867-34.5867 rank 0 POSIX  read  bytes 155607..155985 (conflict overlap 379 bytes)
          time 34.5878-34.5881 rank 2 MPI-IO write bytes 154607..159800 (conflict overlap 1407 bytes)
          time 34.5879-34.5880 rank 2 POSIX  write bytes 154607..159800 (conflict overlap 1407 bytes)
          CONFLICT bytes 187653..189534: write ranks={1} read/write ranks={0}
          time 34.5883-34.5899 rank 0 MPI-IO write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.5884-34.5898 rank 0 POSIX  write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.8040-34.8041 rank 0 MPI-IO read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8041-34.8041 rank 0 POSIX  read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8052-34.8055 rank 1 MPI-IO write bytes 187653..189534 (conflict overlap 1882 bytes)
          time 34.8053-34.8054 rank 1 POSIX  write bytes 187653..189534 (conflict overlap 1882 bytes)
          CONFLICT bytes 189535..191416: write ranks={2} read/write ranks={0}
          time 34.5883-34.5899 rank 0 MPI-IO write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.5884-34.5898 rank 0 POSIX  write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.8040-34.8041 rank 0 MPI-IO read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8041-34.8041 rank 0 POSIX  read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8052-34.8054 rank 2 MPI-IO write bytes 189535..191416 (conflict overlap 1882 bytes)
          time 34.8053-34.8053 rank 2 POSIX  write bytes 189535..191416 (conflict overlap 1882 bytes)
          CONFLICT bytes 191417..193298: write ranks={3} read/write ranks={0}
          time 34.5883-34.5899 rank 0 MPI-IO write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.5884-34.5898 rank 0 POSIX  write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.8040-34.8041 rank 0 MPI-IO read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8041-34.8041 rank 0 POSIX  read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8052-34.8054 rank 3 MPI-IO write bytes 191417..193298 (conflict overlap 1882 bytes)
          time 34.8053-34.8053 rank 3 POSIX  write bytes 191417..193298 (conflict overlap 1882 bytes)
          CONFLICT bytes 193299..195180: write ranks={4} read/write ranks={0}
          time 34.5883-34.5899 rank 0 MPI-IO write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.5884-34.5898 rank 0 POSIX  write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.8040-34.8041 rank 0 MPI-IO read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8041-34.8041 rank 0 POSIX  read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8052-34.8069 rank 4 MPI-IO write bytes 193299..195180 (conflict overlap 1882 bytes)
          time 34.8054-34.8068 rank 4 POSIX  write bytes 193299..195180 (conflict overlap 1882 bytes)
          CONFLICT bytes 195181..197062: write ranks={5} read/write ranks={0}
          time 34.5883-34.5899 rank 0 MPI-IO write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.5884-34.5898 rank 0 POSIX  write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.8040-34.8041 rank 0 MPI-IO read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8041-34.8041 rank 0 POSIX  read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8051-34.8066 rank 5 MPI-IO write bytes 195181..197062 (conflict overlap 1882 bytes)
          time 34.8052-34.8065 rank 5 POSIX  write bytes 195181..197062 (conflict overlap 1882 bytes)
          CONFLICT bytes 197063..198944: write ranks={6} read/write ranks={0}
          time 34.5883-34.5899 rank 0 MPI-IO write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.5884-34.5898 rank 0 POSIX  write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.8040-34.8041 rank 0 MPI-IO read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8041-34.8041 rank 0 POSIX  read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8052-34.8068 rank 6 MPI-IO write bytes 197063..198944 (conflict overlap 1882 bytes)
          time 34.8053-34.8067 rank 6 POSIX  write bytes 197063..198944 (conflict overlap 1882 bytes)
          CONFLICT bytes 198945..200826: write ranks={7} read/write ranks={0}
          time 34.5883-34.5899 rank 0 MPI-IO write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.5884-34.5898 rank 0 POSIX  write bytes 185771..208925 (conflict overlap 1882 bytes)
          time 34.8040-34.8041 rank 0 MPI-IO read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8041-34.8041 rank 0 POSIX  read  bytes 186587..208518 (conflict overlap 1882 bytes)
          time 34.8052-34.8087 rank 7 MPI-IO write bytes 198945..200826 (conflict overlap 1882 bytes)
          time 34.8054-34.8087 rank 7 POSIX  write bytes 198945..200826 (conflict overlap 1882 bytes)


TODO: detect false sharing, where two writers modify non-overlapping data in the same block.
