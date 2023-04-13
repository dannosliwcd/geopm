#!/bin/bash
#SBATCH --job-name=is.c
#SBATCH -N 1
#SBATCH --time=00:02:00

APP='is.C'
WORKDIR="${GEOPM_WORKDIR:-.}/endpoint"
        #--geopm-trace="${WORKDIR}/${APP}.${SLURM_JOBID}.trace" \
        #--geopm-trace-signals=CPU_POWER_LIMIT_CONTROL \
python3 ./balance_client.py \
        geopmlaunch srun -N 1 -n 32 \
        --geopm-report="${WORKDIR}/${APP}.${SLURM_JOBID}.report" \
        --geopm-agent=power_governor \
        -- \
        ${HOME}/geopm/integration/apps/nasis/NPB3.4.2/NPB3.4-MPI/bin/${APP}.x
