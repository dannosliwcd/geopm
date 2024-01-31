#!/bin/bash
#  Copyright (c) 2015 - 2024, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

if [ $# -ne 2 ]; then
    echo Usage: $0 W_URING_DEB  WO_URING_DEB
    echo        W_URING_DEB directory containing GEOPM debian packages built with uring support
    echo        WO_URING_DEB directory containing GEOPM debian packages built without uring support
    exit -1
fi

set -x
set -e

W_URING_DEB=$1
WO_URING_DEB=$2
LOOP_COUNT=100
DELAY=1e-4

# Make sure msr-safe is loaded
sudo modprobe msr-safe
# Install GEOPM service with support for liburing
sudo apt install ${W_URING_DEB}/*
# Use mrs-safe driver through the GEOPM Service shared memory interface
./test_batch_perf ${LOOP_COUNT} ${DELAY} > msr-safe-service.csv
# Use mrs-safe driver directly wihtout GEOPM Service
sudo ./test_batch_perf ${LOOP_COUNT} ${DELAY} > msr-safe-root.csv
sudo chown ${USER} msr-safe-root.csv

# Remove the msr-safe driver
sudo modprobe --remove msr-safe
# Make sure the stock msr driver is loaded
sudo modprobe  msr
sudo systemctl restart geopm
# Use stock msr driver through the GEOPM Service with kernel asynchronous IO
./test_batch_perf ${LOOP_COUNT} ${DELAY} > msr-uring-service.csv
# Use stock msr driver directly with kernel asynchronous IO
sudo ./test_batch_perf ${LOOP_COUNT} ${DELAY} > msr-uring-root.csv

sudo apt install ${WO_URING_DEB}/*

# Use stock msr driver through the GEOPM Service with serial reads
./test_batch_perf ${LOOP_COUNT} ${DELAY} > msr-sync-service.csv
# Use stock msr driver directlye with serial reads
sudo ./test_batch_perf ${LOOP_COUNT} ${DELAY} > msr-sync-root.csv
sudo chown ${USER} msr-safe-root.csv

# Restore msr-safe
sudo modprobe msr-safe
sudo systemctl restart geopm
