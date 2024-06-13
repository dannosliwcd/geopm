#!/bin/bash
#  Copyright (c) 2015 - 2024 Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

cat > temp_config.json << "EOF"
{
    "loop-count": 10,
    "region": ["stream", "dgemm"],
    "big-o": [3.0, 30.0]
}
EOF

source ${GEOPM_SOURCE}/integration/config/run_env.sh

TEST_NAME=test_multi_app
export GEOPM_PROFILE=${TEST_NAME}
export GEOPM_PROGRAM_FILTER=geopmbench,stress-ng
export LD_PRELOAD=libgeopm.so.2.1.0

GEOPM_REPORT=${TEST_NAME}_report.yaml \
GEOPM_REPORT_SIGNALS=TIME@package \
GEOPM_NUM_PROC=2 \
setsid geopmctl &

# Some of the initialization regions of geopmbench finish quickly. The
# test_multi_app.py test checks the execution time of all regions. This sleep
# ensures that geopmctl is started before starting geopmbench.
sleep 2

# geopmbench
export GEOPMBENCH_NO_MPI=1
numactl --cpunodebind=0 -- geopmbench temp_config.json &

# stress-ng
numactl --cpunodebind=1 -- stress-ng --cpu 1 --timeout 10 &

wait $(jobs -p)
