#!/bin/bash
#  Copyright (c) 2015 - 2022, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

set -x
set -e

# Get helper functions
source ../build_func.sh

# Set variables for workload
DIRNAME=lammps-stable_29Oct2020
ARCHIVE=stable_29Oct2020.tar.gz
URL=https://github.com/lammps/lammps/archive/

# Run helper functions
clean_source ${DIRNAME}
get_archive ${ARCHIVE} ${URL}
unpack_archive ${ARCHIVE}
setup_source_git ${DIRNAME}

# Build application
cd ${DIRNAME}/src
make yes-asphere yes-class2 yes-kspace yes-manybody yes-misc yes-molecule yes-mpiio yes-opt yes-replica yes-rigid yes-user-omp yes-user-intel yes-user-sph yes-user-reaxc
make intel_cpu_intelmpi -j 20
