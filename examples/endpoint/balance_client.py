#!/usr/bin/env python3
import asyncio
import os
import sys
import geopmpy.endpoint
import geopmpy.agent
import subprocess
import time
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
    print(f'{agent_name} agent is running profile {endpoint.profile_name()}')
    nodes = endpoint.nodes()
    print('Nodes:', endpoint.nodes())
    print('Policy names:', policy_names)
    print('Sample names:', geopmpy.agent.sample_names(agent_name))

    endpoint.write_policy({'CPU_POWER_LIMIT': 150})

    average_host_power = 100.0
    writer.write(f'{HOST_COUNT}\n{average_host_power}\n'.encode())
    await writer.drain()
    #time.sleep(1.0)

    while proc.poll() is None:
        if do_limit_power:
            # Get the new limit from the cluster-level balancer
            policy_bytes = await reader.readline()
            if not policy_bytes:
                break
            policy = policy_bytes.decode()
            limit = float(policy)
            print(f'{nodes} Received power cap: {limit}')

            # Get a sample from running under our last limit and forward our
            # new limit to the agent
            try:
                sample_age, sample_data = endpoint.read_sample()
                print(f'{nodes} Read a sample (age={sample_age:.5f}):', sample_data)
                endpoint.write_policy({'CPU_POWER_LIMIT': limit})
                print(f'{nodes} Set limit: {limit}')
            except RuntimeError:
                break

            # Forward our sample to the cluster-level balancer
            power_message = f'{sample_data["POWER"]}\n'
            writer.write(power_message.encode())
            await writer.drain()

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
print('rc:', return_code)
sys.exit(return_code)
