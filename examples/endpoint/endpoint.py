#!/usr/bin/env python3

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

NNODES = int(os.environ['SLURM_NNODES'])
HOME = os.environ['HOME']

ENDPOINT_PATH = '/dev/shm/testendpoint'
# TODO: must this reside on a shared file system? Or is local okay if we get the right host?
ENDPOINT_PATH = os.path.join(HOME, 'testendpoint')

REPORT_PATH = 'geopm.report'
TRACE_PATH = 'geopm.trace'
BENCH_CONF_PATH = os.path.join(HOME, 'bench.conf')

try:
    os.remove(f'{ENDPOINT_PATH}-policy')
    os.remove(f'{ENDPOINT_PATH}-sample')
except FileNotFoundError:
    pass
# TODO: CWD needs to be same on both endpoint sides
endpoint = geopmpy.endpoint.Endpoint(ENDPOINT_PATH)
endpoint.open()

proc = subprocess.Popen(['geopmlaunch', 'srun', '-N', f'{NNODES}', '-n', f'{NNODES*2}',
                         f'--geopm-endpoint={ENDPOINT_PATH}',
                         f'--geopm-report={REPORT_PATH}', f'--geopm-trace={TRACE_PATH}',
                         '--geopm-trace-signals=CPU_POWER_LIMIT_CONTROL@board',
                         '--geopm-agent=power_governor', 'geopmbench', BENCH_CONF_PATH])

print('Waiting for an agent to attach...')
endpoint.wait_for_agent_attach(timeout=10)

agent_name = endpoint.agent()
policy_names = geopmpy.agent.policy_names(agent_name)
print(f'{agent_name} agent is running profile {endpoint.profile_name()}')
print('Nodes:', endpoint.nodes())
print('Policy names:', policy_names)
print('Sample names:', geopmpy.agent.sample_names(agent_name))

endpoint.write_policy({'CPU_POWER_LIMIT': 150})
time.sleep(2)

if 'CPU_POWER_LIMIT' in policy_names:
    for i in range(8):
        limit = 150 + (i % 2) * 30
        try:
            sample_age, sample_data = endpoint.read_sample()
            print(f'Read a sample (age={sample_age:.5f}):', sample_data)
            endpoint.write_policy({'CPU_POWER_LIMIT': limit})
            print('Set limit:', limit)
        except RuntimeError:
            break
        time.sleep(2)

proc.wait()

with open(REPORT_PATH) as f:
    report = load(f, Loader=SafeLoader)

for host, host_data in report['Hosts'].items():
    print(f'===== Host {host} time: {host_data["Application Totals"]["runtime (s)"]} =====')
    df = pd.read_csv(f'{TRACE_PATH}-{host}', sep='|', comment='#', na_values=['nan', 'NAN'], index_col='TIME')
    budget_changes = df.loc[df['POWER_BUDGET'].shift(1) != df['POWER_BUDGET'], ['CPU_ENERGY', 'POWER_BUDGET']].copy()
    budget_changes['MEAN_POWER'] = budget_changes['CPU_ENERGY'].diff() / budget_changes.index.to_series().diff()

    print(budget_changes.to_string())

