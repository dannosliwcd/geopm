#!/bin/bash
#  Copyright (c) 2015 - 2024 Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#
# Creates all of the RPM or Debian packages in the repository
die() {
    1>&2 printf "%s\n" "$@"
    exit 1
}

skip_runtime=0
if [ $# == 2 ] && [ "$1" == '--skip-runtime' ]; then
    skip_runtime=1
fi

if grep -i ubuntu /etc/os-release || grep -i debian /etc/os-release; then
    pkg=deb
else
    pkg=rpm
    RPM_TOPDIR=${RPM_TOPDIR:-${HOME}/rpmbuild}
fi

set -e

cd libgeopmd
./autogen.sh
./configure
make $pkg
cd -

cd geopmdpy
./make_$pkg.sh
cd -

cd docs
./make_$pkg.sh
cd -

if [ "$skip_runtime" -eq 0 ]; then
    libgeopmd_version="$(cat libgeopmd/VERSION)"
    if [ -z "${libgeopmd_version}" ]; then
        die "Error: libgeopmd/VERSION is not set"
    fi

    deps_tmp_root="${PWD}/$(mktemp -d libgeopmd-deps-tmp.XXXXXX)"
    if [ "$?" -ne 0 ]; then
        die "Error: failed to create temporary directory for libgeopmd dependencies"
    fi

    if ! pushd "$deps_tmp_root"; then
        die "Error: failed to enter temporary directory for libgeopmd dependencies"
    fi

    if [ "$pkg" == 'deb' ]; then
        for deb_path in ../libgeopmd/*"${libgeopmd_version}"*.deb
        do
            if ! ar x "$deb_path"; then
                die "Error: Unable to unpack libgeopmd version ${libgeopmd_version} DEBs"
            fi

            if ! tar xf data.tar.zst; then
                die "Error: Unable to decompress libgeopmd $deb_path data"
            fi
        done
    elif [ "$pkg" == 'rpm' ]; then
        for rpm_path in "${RPM_TOPDIR}/RPMS/$(uname -m)/"{geopm-service,libgeopmd2}*"${libgeopmd_version}"*.rpm
        do
            if ! rpm2cpio "$rpm_path" | cpio -idmv; then
                die "Error: Unable to unpack libgeopmd version ${libgeopmd_version} RPMs"
            fi
        done
    else
        die "Error: Encountered an unexpected package type: $pkg"
    fi

    popd

    cd libgeopm
    ./autogen.sh
    ./configure --disable-mpi \
                --disable-openmp \
                --with-geopmd-include="${deps_tmp_root}/usr/include" \
                --with-geopmd-lib="$(dirname "$(find "${deps_tmp_root}/usr" -name libgeopmd.so.2 | head -n1)")"
    make $pkg
    cd -

    cd geopmpy
    C_INCLUDE_PATH="${deps_tmp_root}/usr/include" \
        LIBRARY_PATH="$(dirname "$(find "${deps_tmp_root}/usr" -name libgeopmd.so.2 | head -n1)")" \
        ./make_$pkg.sh
    cd -
fi

rm -r "${deps_tmp_root}"
