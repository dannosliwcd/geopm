#!/bin/bash
if [[ $# -ne 1 ]]
then
        1>&2 echo "Usage: $0 <path-to-geopmbench-conf>"
        exit 1
fi
CONF_PATH="$1"

source ~/geopm/integration/config/run_env.sh
python3 ./balance_client.py \
        geopmlaunch srun -N "$SLURM_NNODES" -n $((SLURM_NNODES * 2)) \
        --geopm-report="job.${SLURM_JOBID}.report" \
        --geopm-trace="job.${SLURM_JOBID}.trace" \
        --geopm-agent=power_governor \
        --geopm-trace-signals=CPU_POWER_LIMIT_CONTROL \
        -- \
        geopmbench "${CONF_PATH}"
