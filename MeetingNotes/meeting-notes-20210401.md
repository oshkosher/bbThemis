# April 1st, 2020

### SC21 Paper Discussion

- Experiment Design
  - app I/O visualization:
    [ ] WRF, NAMD, specfem3D, Montage, ResNet, BERT
    
  - Lustre
    [ ] apps: WRF, NAMD, specfem3D, Montage, ResNet, BERT
    - sharing: NAMD-64 and ResNet-16

  - DAOS
    [ ] IOR, mdtest

  - bbThemis
    [ ] IOR, mdtest
    [ ] baseline apps: WRF, NAMD, specfem3D, Montage, ResNet, BERT
    [ ] saturated bbThemis apps (by launching IOR on 64 nodes): WRF, NAMD, specfem3D, Montage, ResNet, BERT
    - sharing: 
      [ ] IOR-32 and IOR-32
      [ ] IOR-32 and IOR-16
      [ ] IOR-32 and IOR-8
      [ ] IOR-32 and IOR-4
      [ ] IOR-32 and an app
      [ ] NAMD-64 and ResNet-16
      [ ] WRF-4, NAMD-64, specfem3D-16, Montage-1, ResNet-16, BERT-4
      [ ] How to test the limitation of sharing? How many jobs can run at the same time without perf loss?
