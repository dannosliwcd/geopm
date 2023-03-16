#!/bin/bash
source ~/geopm/integration/config/run_env.sh
python3 ./balance_client.py \
        geopmlaunch srun -N "$SLURM_NNODES" -n $((SLURM_NNODES * 2)) \
        --geopm-report="job.${SLURM_JOBID}.report" \
        --geopm-trace="job.${SLURM_JOBID}.trace" \
        --geopm-agent=power_governor \
        --geopm-trace-signals=CPU_POWER_LIMIT_CONTROL \
        -- \
        ${HOME}/geopm/integration/apps/minife/miniFE_openmp-2.0-rc3/src/miniFE.x -nx=512 -ny=512 -nz=512 -name=minife
