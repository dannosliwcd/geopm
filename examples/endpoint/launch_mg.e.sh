#!/bin/bash
#SBATCH --job-name=mg.e
#SBATCH -N 4
#SBATCH --time=00:06:44

APP='mg.E'
WORKDIR="${GEOPM_WORKDIR:-.}/endpoint"
        #--geopm-trace="${WORKDIR}/${APP}.${SLURM_JOBID}.trace" \
python3 ./balance_client.py \
        geopmlaunch srun -N 4 -n 128 \
        --geopm-report="${WORKDIR}/${APP}.${SLURM_JOBID}.report" \
        --geopm-profile="${APP}.x${GEOPM_PROFILE_SUFFIX}" \
        --geopm-agent=power_governor \
        --geopm-trace-signals=CPU_POWER_LIMIT_CONTROL \
        -- \
        ${HOME}/geopm/integration/apps/nasmg/NPB3.4.2/NPB3.4-MPI/bin/${APP}.x
