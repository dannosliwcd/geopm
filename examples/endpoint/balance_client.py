#!/usr/bin/env python3
import asyncio
import os
import sys
import geopmpy.endpoint
import geopmpy.agent
import subprocess
import time
import math
from datetime import datetime
import os
import pandas as pd

from yaml import load
try:
    from yaml import CSafeLoader as SafeLoader
except ImportError:
    from yaml import SafeLoader

if len(sys.argv) > 0 and sys.argv[1] in ['-h', '--help']:
    print(f'Usage: {sys.argv[0]} <geomplaunch command to run>\n\n'
          'Launches geopm and sets environment variables to use the cross-job power capper '
          'as a GEOPM endpoint. The launched command must use a GEOPM agent that accepts '
          'CPU_POWER_LIMIT in its policy and sends POWER in its sample data.')
    sys.exit(0)

INITIAL_POWER_LIMIT = 280
GEOPM_ENDPOINT_SOCKET = 63094  # should be configurable
#                     ("GEOPM")
GEOPM_ENDPOINT_SERVER_HOST = os.getenv('GEOPM_ENDPOINT_SERVER_HOST')
if GEOPM_ENDPOINT_SERVER_HOST is None:
    print('Error: Must set env var GEOPM_ENDPOINT_SERVER_HOST', file=sys.stderr)
    sys.exit(1)

HOST_COUNT = os.getenv("SLURM_NNODES")
if HOST_COUNT is None:
    print('Error: Expected SLURM_NNODES to be set', file=sys.stderr)
    sys.exit(1)
HOST_COUNT = int(HOST_COUNT)

JOBID = os.getenv("SLURM_JOBID")
if JOBID is None:
    print('Error: Expected SLURM_JOBID to be set', file=sys.stderr)
    sys.exit(1)

GEOPM_ENDPOINT_TRACE = os.getenv('GEOPM_ENDPOINT_TRACE')
trace_file = None
if GEOPM_ENDPOINT_TRACE is not None:
    trace_file = open(f'{GEOPM_ENDPOINT_TRACE}-{JOBID}', 'w')


HOME = os.environ['HOME']
# TODO: must this reside on a shared file system? Or is local okay if we get the right host?
ENDPOINT_PATH = os.path.join(HOME, 'testendpoint')

async def tcp_policy_sample(argv):
    reader, writer = await asyncio.open_connection(
        GEOPM_ENDPOINT_SERVER_HOST, GEOPM_ENDPOINT_SOCKET)
    wrappedpolicy = ''

    # Clean up any previous endpoints
    try:
        os.remove(f'{ENDPOINT_PATH}-{JOBID}-policy')
        os.remove(f'{ENDPOINT_PATH}-{JOBID}-sample')
    except FileNotFoundError:
        pass

    # Create the new endpoint for this job/step
    endpoint = geopmpy.endpoint.Endpoint(f'{ENDPOINT_PATH}-{JOBID}')
    endpoint.open()

    env = os.environ.copy()
    env['GEOPM_ENDPOINT'] = f'{ENDPOINT_PATH}-{JOBID}'
    proc = subprocess.Popen(argv, env=env)
    print('Starting:', ' '.join(argv))
    # TODO: Start the endpoint and subprocess.Popen() the job step. Get host count from that and lose HOST_COUNT env var stuff

    print('Waiting for an agent to attach...')
    do_limit_power = True
    endpoint.wait_for_agent_attach(timeout=10)
    agent_name = endpoint.agent()
    policy_names = geopmpy.agent.policy_names(agent_name)
    if 'CPU_POWER_LIMIT' not in policy_names:
        print('Error: Agent does not accept CPU_POWER_LIMIT in its policy. '
              'No power capping will be applied.', file=sys.stderr)
        do_limit_power = False
    sample_names = geopmpy.agent.sample_names(agent_name)
    if 'POWER' in sample_names:
        SAMPLE_NAME = 'POWER'
    elif 'SUM_POWER_SLACK' in sample_names:
        SAMPLE_NAME = 'SUM_POWER_SLACK'

    FORWARD_SAMPLES = []
    if 'MAX_EPOCH_RUNTIME' in sample_names:
        FORWARD_SAMPLES.append('MAX_EPOCH_RUNTIME')
    if 'SAMPLE_COUNT' in sample_names:
        FORWARD_SAMPLES.append('SAMPLE_COUNT')

    print(f'{agent_name} agent is running profile {endpoint.profile_name()}')
    nodes = endpoint.nodes()
    print('Nodes:', endpoint.nodes())
    print('Policy names:', policy_names)
    print('Sample names:', geopmpy.agent.sample_names(agent_name))
    if trace_file is not None:
        print(f'# Nodes: {endpoint.nodes()}', file=trace_file)
        print(f'# Agent: {agent_name}', file=trace_file)
        print(f'# Profile: {endpoint.profile_name()}', file=trace_file)
        print(f'# Start Time: {datetime.now().strftime("%a %b %d %H:%M:%S.%f %Y")}', file=trace_file)
        trace_start = time.time()
        print('time,cap,epoch,epoch duration,progress cap,progress,progress duration', file=trace_file)
        print(f'0,{INITIAL_POWER_LIMIT},nan,nan,{INITIAL_POWER_LIMIT},nan,nan', file=trace_file)

    endpoint.write_policy({'CPU_POWER_LIMIT': INITIAL_POWER_LIMIT})

    # TODO: read power instead of sending INITIAL_POWER_LIMIT?
    writer.write(f'{len(endpoint.nodes())}\n{INITIAL_POWER_LIMIT}\n{endpoint.profile_name()}\n'.encode())
    await writer.drain()
    last_sample_time = time.time()

    # Used to get the time-weighted mean of power limits over an epoch
    epoch_energy_limit = 0
    epoch_energy_duration = 0

    progress_update_energy_limit = 0
    progress_update_energy_duration = 0

    last_epoch_time = float('nan')
    last_progress_update_time = float('nan')
    last_epoch_count = float('nan')
    last_progress = float('nan')

    while proc.poll() is None:
        if do_limit_power:
            # Get the new limit from the cluster-level balancer
            policy_bytes = await reader.readline()
            if not policy_bytes:
                break
            policy = policy_bytes.decode()
            power_limit = float(policy)
            print(f'{nodes} Received power cap: {power_limit}')

            # Get a sample from running under our last limit and forward our
            # new limit to the agent
            try:
                sample_age, sample_data = endpoint.read_sample()
                sample_time = time.time() - sample_age
                print(f'{nodes} Read a sample (age={sample_age:.5f}):', sample_data)
                policy = {'CPU_POWER_LIMIT': power_limit}
                for sample_name in FORWARD_SAMPLES:
                    policy[sample_name] = sample_data[sample_name]
                if 'STEP_COUNT' in policy:
                    policy['STEP_COUNT'] += 1
                if 'SUM_POWER_SLACK' in sample_data:
                    policy['POWER_SLACK'] = sample_data['SUM_POWER_SLACK']
                endpoint.write_policy(policy)
                print(f'{nodes} Set limit: {power_limit}')
            except RuntimeError:
                break

            time_since_last_sample = sample_time - last_sample_time

            # Forward our sample to the cluster-level balancer
            epoch = sample_data["EPOCH_COUNT"]
            epoch_start_time = sample_data["EPOCH_START_TIME"]
            epoch_energy_limit += power_limit * time_since_last_sample
            epoch_energy_duration += time_since_last_sample
            average_epoch_cap = float('nan')
            epoch_duration = float('nan')
            do_print_trace = False
            if epoch != last_epoch_count and not math.isnan(epoch) and epoch_energy_duration > 0:
                do_print_trace = True
                epoch_duration = epoch_start_time - last_epoch_time
                average_epoch_cap = epoch_energy_limit / epoch_energy_duration
                epoch_energy_limit = 0
                epoch_energy_duration = 0
                last_epoch_time = epoch_start_time
                last_epoch_count = epoch

            progress = sample_data["PROGRESS"]
            progress_update_time = sample_data["PROGRESS_UPDATE_TIME"]
            progress_update_energy_limit += power_limit * time_since_last_sample
            progress_update_energy_duration += time_since_last_sample
            average_progress_update_cap = float('nan')
            progress_duration = float('nan')
            if progress != last_progress and not math.isnan(progress) and progress_update_energy_duration > 0:
                do_print_trace = True
                progress_duration = progress_update_time - last_progress_update_time
                average_progress_update_cap = progress_update_energy_limit / progress_update_energy_duration
                progress_update_energy_limit = 0
                progress_update_energy_duration = 0
                last_progress_update_time = progress_update_time
                last_progress_update = progress

            if do_print_trace:
                # Either the epoch or progress samples have been updated
                if trace_file is not None:
                    print(f'{sample_time - trace_start},{average_epoch_cap},{epoch},{epoch_duration},{average_progress_update_cap},{progress},{progress_duration}', file=trace_file)

            power_message = f'{sample_data[SAMPLE_NAME]},{epoch},{average_epoch_cap},{epoch_duration},{progress},{average_progress_update_cap},{progress_duration}\n'
            writer.write(power_message.encode())
            await writer.drain()

            last_sample_time = sample_time

    print('All done!')
    writer.close()
    proc.poll()
    return proc.returncode


if len(sys.argv) > 1:
    wrapped_argv = sys.argv[1:]
else:
    wrapped_argv = ['geopmlaunch', 'srun', '-N', str(HOST_COUNT), '-n', str(HOST_COUNT*2),
                    f'--geopm-report=job.{JOBID}.report',
                    f'--geopm-trace=job.{JOBID}.trace',
                    '--geopm-agent=power_governor',
                    '--geopm-trace-signals=CPU_POWER_LIMIT_CONTROL',
                    '--', 'geopmbench', os.path.join(HOME, 'bench.conf')]
loop = asyncio.get_event_loop()
return_code = loop.run_until_complete(tcp_policy_sample(wrapped_argv))
loop.close()
if trace_file is not None:
    trace_file.close()
print('rc:', return_code)
sys.exit(return_code)
