#!/bin/bash
#  Copyright (c) 2015 - 2024 Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

NODE_RESOURCE="geopm-node-power-limit"
NODE_MAX_RESOURCE="geopm-max-node-power-limit"
NODE_MIN_RESOURCE="geopm-min-node-power-limit"
GEOPM_PBS_SCRIPT_DIR="$(dirname "$(readlink -f $0)")"
NODE_CAP_HOOK="geopm_power_limit"
REMOVE_OPT="--remove"
SAVED_CONTROLS_BASE_DIR="/run/geopm/pbs-hooks"

# Set the location of PBS_HOME by sourcing the PBS config.
source '/etc/pbs.conf'

print_usage() {
    echo "
    Usage: $0 [${REMOVE_OPT}]

    Invoking this script with no arguments installs the GEOPM power limit
    hook.

    Use the --remove option to uninstall the hook. This will also remove the
    $NODE_RESOURCE resource and the saved controls directory.
    "
}

install() {
    for resource in $NODE_RESOURCE $NODE_MAX_RESOURCE $NODE_MIN_RESOURCE
    do
        out=`qmgr -c "list resource" | grep "$resource"`
        if [ -z "$out" ]; then
            echo "Creating $resource resource..."
            qmgr -c "create resource $resource type=float" || exit 1
        else
            echo "$resource resource already exists"
        fi
    done
    echo "Marking $NODE_MAX_RESOURCE and $NODE_MIN_RESOURCE as read-only resources..."
    qmgr -c "set resource $NODE_MAX_RESOURCE flag=r" || exit 1
    qmgr -c "set resource $NODE_MIN_RESOURCE flag=r" || exit 1

    out=`qmgr -c "list hook" | grep "$NODE_CAP_HOOK"`
    if [ -z "$out" ]; then
        echo "Creating $NODE_CAP_HOOK hook..."
        qmgr -c "create hook $NODE_CAP_HOOK" || exit 1
    else
        echo "$NODE_CAP_HOOK hook already exists"
    fi
    echo "Importing and configuring hook..."
    qmgr -c "import hook $NODE_CAP_HOOK application/x-python default ${GEOPM_PBS_SCRIPT_DIR}/${NODE_CAP_HOOK}.py" || exit 1
    qmgr -c "set hook $NODE_CAP_HOOK event='queuejob,execjob_prologue,execjob_epilogue'" || exit 1

    echo "Done."
}

remove() {
    for resource in $NODE_RESOURCE $NODE_MAX_RESOURCE $NODE_MIN_RESOURCE
    do
        out=`qmgr -c "list resource" | grep "$resource"`
        if [ -z "$out" ]; then
            echo "$resource resource not found"
        else
            echo "Removing $resource resource..."
            qmgr -c "delete resource $resource" || exit 1
        fi
    done

    out=`qmgr -c "list hook" | grep "$NODE_CAP_HOOK"`
    if [ -z "$out" ]; then
        echo "$NODE_CAP_HOOK hook not found"
    else
        echo "Removing $NODE_CAP_HOOK hook..."
        qmgr -c "delete hook $NODE_CAP_HOOK" || exit 1
    fi

    rm -rf "$SAVED_CONTROLS_BASE_DIR"

    echo "Done."
}

if [ $# -eq 0 ]; then
    install
    echo "GEOPM PBS plugins have been installed but are not yet configured."
    echo "To set a minimum node power limit, use $NODE_MIN_RESOURCE"
    echo "To set a maximum node power limit, use $NODE_MAX_RESOURCE"
elif [ $# -eq 1 ]; then
    if [ "$1" == "$REMOVE_OPT" ]; then
        remove
    else
        echo "Unrecognized option: $1"
        print_usage
    fi
else
    echo "Invalid number of arguments"
    print_usage
fi
