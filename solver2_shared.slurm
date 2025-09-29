#!/bin/bash

#SBATCH --job-name=solver2_shared
#SBATCH --time=0:20:0
#SBATCH --exclusive
#SBATCH --nodes=1
#SBATCH --tasks-per-node=1
#SBATCH --cpus-per-task=32
#SBATCH --account=
#SBATCH --partition=standard
#SBATCH --qos=standard
#SBATCH --output=bin/%x-%j.out

module --silent load intel-20.4/compilers
module --silent load mpt

cd $SLURM_SUBMIT_DIR

export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK
export SRUN_CPUS_PER_TASK=$SLURM_CPUS_PER_TASK

srun --cpu-bind=cores ./bin/solver2_shared