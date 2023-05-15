#!/bin/bash
#SBATCH --job-name=cg.d
#SBATCH -N 1
#SBATCH --time=00:14:10

APP='cg.D'
WORKDIR="${GEOPM_WORKDIR:-.}/endpoint"
        #--geopm-trace="${WORKDIR}/${APP}.${SLURM_JOBID}.trace" \
python3 ./balance_client.py \
        geopmlaunch srun -N 1 -n 32 \
        --geopm-report="${WORKDIR}/${APP}.${SLURM_JOBID}.report" \
        --geopm-profile="${APP}.x${GEOPM_PROFILE_SUFFIX}" \
        --geopm-agent=power_governor \
        --geopm-trace-signals=CPU_POWER_LIMIT_CONTROL \
        -- \
        ${HOME}/geopm/integration/apps/nascg/NPB3.4.2/NPB3.4-MPI/bin/${APP}.x
