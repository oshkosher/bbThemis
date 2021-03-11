#Jan 28th, 2020

### SC21 Paper Planning

- System prototype, Lei

- Paper Draft, Zhao

- Theme: To enable intra-io-node burst buffer sharing, incorporatings the consistency models

- Experiments, on large scale, application suite
	- Baseline: Intel DAOS, reference implementation of existing work, multi- vs. single-node burst buffer

- Comments.
	- Dan: some apps may be slowed, local optimization vs. global optimizaiton. tradeoff study.
	- Ed: to penalize application with suboptimal I/O operations, e.g., 4 byte write.

### A Consistency Model Analysis Paper

- CLUSTER May 17th?, FAST in Sep?

- Theme: Consistency Model Relaxing and its impact on program crash and file system workload.
