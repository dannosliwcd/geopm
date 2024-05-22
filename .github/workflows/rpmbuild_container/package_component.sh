#!/bin/sh
#
#  Copyright (c) 2015 - 2024, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#
LOCAL_REPO=/local_repo

print_usage()
{
	echo "Usage: $0 {libgeopmd|libgeopm|geopmpy|geopmdpy|docs}" >&2
}

if [ "$#" -ne 1 ]
then
	print_usage
	exit 1
fi

if ! [ -e "/geopm" ]; then
	echo "/geopm not found. Ensure that the geopm source directory is mounted there." >&2
	exit 1
fi

# Set rpm_topdir to be used by `make rpm` and `make_rpm.sh`
export rpm_topdir=/geopm_rpmbuild
export RPM_TOPDIR="$rpm_topdir"
if ! [ -e "$rpm_topdir" ]; then
	echo "$rpm_topdir not found. Ensure that the RPM build directory is mounted there." >&2
	exit 1
fi

set -e

component="$1"
case "$component" in
	libgeopmd)
		cd /geopm/libgeopmd
		./autogen.sh
		./configure --disable-mpi --disable-openmp --disable-io-uring
		rpmbuild_flags='--define "disable_io_uring 1"' make rpm
		;;
	libgeopm)
		echo "==== Installing prerequisites ===="
		libgeopmd_version=$(cat /geopm/libgeopmd/VERSION)
		mkdir "${LOCAL_REPO}"
		cp /geopm_rpmbuild/RPMS/x86_64/*"${libgeopmd_version}"*.rpm "${LOCAL_REPO}"
		createrepo "${LOCAL_REPO}"
		zypper addrepo --priority 1 --no-gpgcheck "${LOCAL_REPO}"/ geopm
		zypper --non-interactive install geopm-service-devel
		cd /geopm/libgeopm
		./autogen.sh
		./configure --disable-mpi --disable-openmp
		make rpm
		;;
	geopmdpy)
		cd /geopm/geopmdpy
		./make_rpm.sh
		;;
	geopmpy)
		cd /geopm/geopmpy
		./make_rpm.sh
		;;
	docs)
		echo "==== Installing prerequisites ===="
		libgeopmd_version=$(cat /geopm/libgeopmd/VERSION)
		mkdir "${LOCAL_REPO}"
		cd "${LOCAL_REPO}"
		cp /geopm_rpmbuild/RPMS/x86_64/*"${libgeopmd_version}"*.rpm "${LOCAL_REPO}"
		createrepo "${LOCAL_REPO}"
		zypper addrepo --priority 1 --no-gpgcheck "${LOCAL_REPO}"/ geopm
		zypper --non-interactive install python3-geopmdpy
		# Can't zypper install python3-geopmpy in SLES15. The numpy dep is too new.
		pip install /geopm/geopmpy
		pip install pyyaml -r /geopm/docs/requirements.txt
		cd /geopm/docs
		./make_rpm.sh
		;;
	*)
		echo "Unexpected build component: $component" 2>&1
		print_usage
		exit 1
		;;
esac

