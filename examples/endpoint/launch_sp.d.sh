#!/bin/bash
#SBATCH --job-name=sp.d
#SBATCH -N 2
#SBATCH --time=00:25:00

APP='sp.D'
WORKDIR="${GEOPM_WORKDIR:-.}/endpoint"
        #--geopm-trace="${WORKDIR}/${APP}.${SLURM_JOBID}.trace" \
python3 ./balance_client.py \
        geopmlaunch srun -N 2 -n 81 \
        --geopm-report="${WORKDIR}/${APP}.${SLURM_JOBID}.report" \
        --geopm-profile="${APP}.x${GEOPM_PROFILE_SUFFIX}" \
        --geopm-agent=power_governor \
        --geopm-trace-signals=CPU_POWER_LIMIT_CONTROL \
        -- \
        ${HOME}/geopm/integration/apps/nassp/NPB3.4.2/NPB3.4-MPI/bin/${APP}.x
