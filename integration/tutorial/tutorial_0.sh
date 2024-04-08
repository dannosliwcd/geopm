#!/bin/bash
#
#  Copyright (c) 2015 - 2024, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

set err=0
. tutorial_env.sh

export PATH=$GEOPM_BIN:$PATH
export PYTHONPATH=$GEOPMPY_PKGDIR:$PYTHONPATH
export LD_LIBRARY_PATH=$GEOPM_LIB:$LD_LIBRARY_PATH

# Run on 4 nodes
# with 8 total application MPI ranks
# load geopm runtime with LD_PRELOAD
# launch geopm controller as an MPI process
# create a report file
# create trace files

NUM_NODES=4
RANKS_PER_NODE=4
TOTAL_RANKS=$((${RANKS_PER_NODE} * ${NUM_NODES}))
HOSTNAME=$(hostname)

if [ "$MPIEXEC_CTL" -a "$MPIEXEC_APP" ]; then
    export LD_PRELOAD=$GEOPM_LIB/libgeopm.so
    export LD_DYNAMIC_WEAK=true
    export GEOPM_PROGRAM_FILTER=tutorial_0

    # Use MPIEXEC_CTL to launch the Controller process on all nodes
    GEOPM_REPORT=tutorial_0_report_${HOSTNAME} \
    GEOPM_TRACE=tutorial_0_trace \
    GEOPM_NUM_PROC=${RANKS_PER_NODE} \
    $MPIEXEC_CTL geopmctl &

    # Use MPIEXEC_APP to launch the job
    $MPIEXEC_APP ./tutorial_0
    err=$?
elif [ "$GEOPM_LAUNCHER" = "srun" ]; then
    # Use GEOPM launcher wrapper script with SLURM's srun
    geopmlaunch srun \
                -N ${NUM_NODES} \
                -n ${TOTAL_RANKS} \
                --geopm-preload \
                --geopm-ctl=application \
                --geopm-report=tutorial_0_report_${HOSTNAME} \
                --geopm-trace=tutorial_0_trace \
                --geopm-program-filter=tutorial_0 \
                --geopm-affinity-enable \
                -- ./tutorial_0
    err=$?
elif [ "$GEOPM_LAUNCHER" = "aprun" ]; then
    # Use GEOPM launcher wrapper script with ALPS's aprun
    geopmlaunch aprun \
                -N ${RANKS_PER_NODE} \
                -n ${TOTAL_RANKS} \
                --geopm-preload \
                --geopm-ctl=application \
                --geopm-report=tutorial_0_report_${HOSTNAME} \
                --geopm-trace=tutorial_0_trace \
                --geopm-program-filter=tutorial_0 \
                --geopm-affinity-enable \
                -- ./tutorial_0
    err=$?
elif [ "$GEOPM_LAUNCHER" = "impi" ]; then
    # Use GEOPM launcher wrapper script with Intel(R) MPI
    geopmlaunch impi \
                -ppn ${RANKS_PER_NODE} \
                -n ${TOTAL_RANKS} \
                --geopm-preload \
                --geopm-ctl=application \
                --geopm-report=tutorial_0_report_${HOSTNAME} \
                --geopm-trace=tutorial_0_trace \
                --geopm-program-filter=tutorial_0 \
                --geopm-affinity-enable \
                -- ./tutorial_0
    err=$?
elif [ "$GEOPM_LAUNCHER" = "ompi" ]; then
    # Use GEOPM launcher wrapper script with Open MPI
    geopmlaunch ompi \
                --npernode ${RANKS_PER_NODE} \
                -n ${TOTAL_RANKS} \
                --hostfile tutorial_hosts \
                --geopm-preload \
                --geopm-ctl=application \
                --geopm-report=tutorial_0_report_${HOSTNAME} \
                --geopm-trace=tutorial_0_trace \
                --geopm-program-filter=tutorial_0 \
                --geopm-affinity-enable \
                -- ./tutorial_0
    err=$?
elif [ "$GEOPM_LAUNCHER" = "pals" ]; then
    # Use GEOPM launcher wrapper script with PALS
    geopmlaunch pals \
                -ppn ${RANKS_PER_NODE} \
                -n ${TOTAL_RANKS} \
                --geopm-preload \
                --geopm-ctl=application \
                --geopm-report=tutorial_0_report_${HOSTNAME} \
                --geopm-trace=tutorial_0_trace \
                --geopm-program-filter=tutorial_0 \
                --geopm-affinity-enable \
                -- ./tutorial_0
    err=$?
else
    echo "Error: $0: Set either GEOPM_LAUNCHER or MPIEXEC_CTL/MPIEXEC_APP in your environment." 2>&1
    echo "       If no resource manager is available, set the MPIEXEC_* variables as follows:" 2>&1
    echo "       MPIEXEC_CTL: Run on 2 nodes, 1 process per node (2 total processes)" 2>&1
    echo "       MPIEXEC_APP: Run on 2 nodes, 4 processes per node (8 total processes)" 2>&1
    echo "" 2>&1
    echo "       E.g.:" 2>&1
    echo "       MPIEXEC_CTL=\"mpiexec -n 2 -ppn 1\" MPIEXEC_APP=\"mpiexec -n 8 -ppn 4\" $0" 2>&1
    err=1
fi

exit $err
