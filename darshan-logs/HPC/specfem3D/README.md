# Benchmark Description
This is a tiny benchmark of a simulation over 1x64x64 grid and 1 minutie of seismic record. 
ADIOS running on top of MPI-IO is used in this run. 
Note that the Specfem3D simulation runs with two steps, the first step is called `xmeshfem3D`, which generates the cube-sphere mesh for the simulation.
Then, the `xspecfem3D` runs the actual simulation with that mesh. 
In a typical seismic research, the `xmeshfem3D` usually runs once, but the `xspecfem3D` runs multiple times especially in a inversion workflow.
Here, we recorded the darshan logs for both application, but it is not necessarily required to analyse the IO dependency between `xmeshfem3D` and `xspecfem3D`.
