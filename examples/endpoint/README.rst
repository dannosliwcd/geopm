GEOPM Endpoint Examples
=======================
This directory includes files to demonstrate usage of the GEOPM endpoint to
provide time-varying power caps.

The following script is a template to execute cluster power capping experiments.

.. code:: bash

    #!/bin/bash
    export GEOPM_ENDPOINT_SERVER_HOST='<EXAMPLE_HEAD_NODE_DOMAIN_NAME>'
    export GEOPM_WORKDIR='<EXAMPLE_WRITABLE_DIRECTORY>'
    export GEOPM_ENDPOINT_TRACE=$GEOPM_WORKDIR/endpoint/trace
    mkdir -p "$GEOPM_WORKDIR/endpoint"
    export EXPERIMENT_TOTAL_NODES=4 # Change to the count of nodes you want to use
    MEAN_POWER_WATTS=210 # Change to the mean power target you want to follow
    RESERVE_WATTS=0 # zero for a static power cap, greater for time-varying caps
    
    ./balance_server.py \
            --average-power-target=$((MEAN_POWER_WATTS * EXPERIMENT_TOTAL_NODES)) \
            --reserve=${RESERVE_WATTS} \
            --use-pre-characterized \
            &
    # Key additional balance_server.py options:
    #  Use --ignore-run-time-models to test without execution-time performance modeling
    #  Use --no-cross-job-sharing to test a uniform power limit across jobs
    #  See more in `balance_server.py -h`

    server_pid=$!
    trap 'kill $server_pid' EXIT
    
    # Set up 6 back-to-back instances of BT on one set of nodes
    for i in {1..6}; do
            sbatch -w 'mycomputenode[5-6]' ./launch_bt.d.sh
    done

    # Set up 6 back-to-back instances of BT on a different set of nodes.
    # This set is misclassified as IS.
    for i in {1..6}; do
            GEOPM_PROFILE_SUFFIX='=is.D.x' sbatch -w 'mycomputenodes[7-8]' ./launch_bt.d.sh
    done
    
    wait

Key files of interest in this directory:

- ``launch_*.sh`` are wrapper scripts to launch various job types.
- ``balance_client.py`` is the endpoint instance that runs within each job.
- ``balance_server.py`` is the endpoint instance that runs on a single management node (e.g., head node)
- ``app_properties.yaml`` defines pre-characterized models for use with the
  ``--use-pre-characterized`` option in the server script. Obtain coefficients
  by running a power sweep on the target application (see
  ``integration/experiment/run_experiment.py -h``) then use
  ``<GEOPM_REPO>/integration/experiment/power_sweep/gen_plot_power_performance_curves.py``
  on the generated report files.

