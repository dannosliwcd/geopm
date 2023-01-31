#!/usr/bin/env python3

import geopmpy.endpoint
import geopmpy.agent
import time
import os

try:
    os.remove('testendpoint-policy')
    os.remove('testendpoint-sample')
except FileNotFoundError:
    pass
# TODO: CWD needs to be same on both endpoint sides
endpoint = geopmpy.endpoint.Endpoint('testendpoint')
endpoint.open()
print('Waiting for an agent to attach...')
endpoint.wait_for_agent_attach(timeout=10)

# geopmlaunch srun -N 1 -n 2 --geopm-endpoint=testendpoint --geopm-trace=geopm.trace --geopm-trace-signals=CPU_POWER_LIMIT_CONTROL@board --geopm-agent=power_governor geopmbench  ~/bench.conf

agent_name = endpoint.agent()
policy_names = geopmpy.agent.policy_names(agent_name)
print(f'{agent_name} agent is running profile {endpoint.profile_name()}')
print('Nodes:', endpoint.nodes())
print('Policy names:', policy_names)
print('Sample names:', geopmpy.agent.sample_names(agent_name))

endpoint.write_policy({'CPU_POWER_LIMIT': 150})
time.sleep(1)

if 'CPU_POWER_LIMIT' in policy_names:
    for i in range(8):
        limit = 150 + (i % 2) * 30
        try:
            sample_age, sample_data = endpoint.read_sample()
            print(f'Read a sample (age={sample_age:.5f}):', sample_data)
            endpoint.write_policy({'CPU_POWER_LIMIT': limit})
            print('Set limit:', limit)
        except RuntimeError:
            print('Encountered an error. Exiting.')
            break
        time.sleep(2)
