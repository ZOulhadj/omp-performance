# TP Coursework 2
This repository contains the source code for Threaded Programming Coursework 2. Included are the following OpenMP programs:
- Solver 1 (Recursive Tasks)
- Solver 2 (Shared Queue)
- Solver 2 (Separate Queues)

The programs use a divide-and-conquer algorithm for an adaptive quadrature method that computes the integral of a function on a closed interval.  The
algorithm starts by applying two quadrature rules (3-point and 5-point Simpson’s
rules) to the whole interval. If the difference between the integral estimates from
the two rules is small enough (or the interval is too short), the result in added to
the total integral estimate. If it is not small enough, the interval is split into two
equal halves, and the method is applied recursively to each halves, and the method is applied recursively to each half. The evaluating the function requires the solution of an ODE (ordinary differential equation) which is relatively expensive in time.

# Building

## Requirements
- OpenMP (201511)
- ICC 20.4 or GCC 10.2.0
- Slurm v22.05.11
- C99


To build all programs run the following command:
```
make -j
```

# Running
Each program can be executed from the root directory:
```
./bin/solver1
./bin/solver2_shared
./bin/solver2_separate
```

# Running on Cirrus
Each program can be submitted to Cirrus using Slurm.

*Note: Make sure that you set you account code in the slurm script prior to submitting the job.*

```
sbatch solver1.slurm
sbatch solver2_shared.slurm
sbatch solver2_separate.slurm
```

Once a job has completed a new Slurm log file will be generated with a ```.out``` extension in the bin directory:
```
./bin/solver1-[id].out
./bin/solver2_shared-[id].out
./bin/solver2_separate-[id].out
```



