#!/bin/bash
if [[ "$SLURM_NNODES" -eq 1 ]]
then
        NPROCS=36
elif [[ "$SLURM_NNODES" -eq 2 ]]
then
        NPROCS=81
elif [[ "$SLURM_NNODES" -eq 3 ]]
then
        NPROCS=121
elif [[ "$SLURM_NNODES" -eq 4 ]]
then
        NPROCS=169
elif [[ "$SLURM_NNODES" -eq 5 ]]
then
        NPROCS=196
elif [[ "$SLURM_NNODES" -eq 6 ]]
then
        NPROCS=256
elif [[ "$SLURM_NNODES" -eq 7 ]]
then
        NPROCS=289
elif [[ "$SLURM_NNODES" -eq 8 ]]
then
        NPROCS=324
else
        1>&2 echo "Fixme: Just implement in the geopm integration infra"
        exit 1
fi

source ~/geopm/integration/config/run_env.sh
python3 ./balance_client.py \
        geopmlaunch srun -N "$SLURM_NNODES" -n "$NPROCS" \
        --geopm-report="job.${SLURM_JOBID}.report" \
        --geopm-trace="job.${SLURM_JOBID}.trace" \
        --geopm-agent=power_governor \
        --geopm-trace-signals=CPU_POWER_LIMIT_CONTROL \
        -- \
        ${HOME}/geopm/integration/apps/nassp/NPB3.4.2/NPB3.4-MPI/bin/sp.D.x
