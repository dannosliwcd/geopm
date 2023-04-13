#!/bin/bash
#SBATCH --job-name=lu.d
#SBATCH -N 1
#SBATCH --time=00:21:18

APP='lu.D'
WORKDIR="${GEOPM_WORKDIR:-.}/endpoint"
        #--geopm-trace="${WORKDIR}/${APP}.${SLURM_JOBID}.trace" \
python3 ./balance_client.py \
        geopmlaunch srun -N 1 -n 42 \
        --geopm-report="${WORKDIR}/${APP}.${SLURM_JOBID}.report" \
        --geopm-agent=power_governor \
        --geopm-trace-signals=CPU_POWER_LIMIT_CONTROL \
        -- \
        ${HOME}/geopm/integration/apps/naslu/NPB3.4.2/NPB3.4-MPI/bin/${APP}.x
