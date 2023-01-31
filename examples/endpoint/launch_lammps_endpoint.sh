#!/bin/bash
source ~/geopm/integration/config/run_env.sh
python3 ./balance_client.py \
        geopmlaunch srun -N "$SLURM_NNODES" -n $((SLURM_NNODES * 42)) \
        --geopm-report="job.${SLURM_JOBID}.report" \
        --geopm-trace="job.${SLURM_JOBID}.trace" \
        --geopm-agent=power_governor \
        --geopm-trace-signals=CPU_POWER_LIMIT_CONTROL \
        -- \
        ${HOME}/geopm/integration/apps/lammps/lammps-stable_29Oct2020/src/lmp_intel_cpu_intelmpi -in in.rhodo.scaled -v x 14 -v y 14 -v z 14 -log none -pk intel 0 omp 2 -sf intel
