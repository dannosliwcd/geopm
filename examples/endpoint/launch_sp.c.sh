#!/bin/bash
#SBATCH --job-name=sp.c
#SBATCH -N 1
#SBATCH --time=00:22:00

APP='sp.C'
WORKDIR="${GEOPM_WORKDIR:-.}/endpoint"
        #--geopm-trace="${WORKDIR}/${APP}.${SLURM_JOBID}.trace" \
python3 ./balance_client.py \
        geopmlaunch srun -N 1 -n 36 \
        --geopm-report="${WORKDIR}/${APP}.${SLURM_JOBID}.report" \
        --geopm-profile="${APP}.x${GEOPM_PROFILE_SUFFIX}" \
        --geopm-agent=power_governor \
        --geopm-trace-signals=CPU_POWER_LIMIT_CONTROL \
        -- \
        ${HOME}/geopm/integration/apps/nassp/NPB3.4.2/NPB3.4-MPI/bin/${APP}.x
