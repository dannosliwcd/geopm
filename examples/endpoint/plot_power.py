#!/usr/bin/env python3
import argparse
from datetime import datetime
from yaml import load
from matplotlib import pyplot as plt
import pandas as pd
import seaborn as sns
try:
    from yaml import CSafeLoader as SafeLoader
except ImportError:
    from yaml import SafeLoader

parser = argparse.ArgumentParser('Generate a power plot')
parser.add_argument('report_paths', nargs='+')
parser.add_argument('--plot-path', default='power.png')

args = parser.parse_args()

active_node_changes = list()
host_traces = list()
hosts = list()

# Reshape to give binned timestamp

for report_path in args.report_paths:
    with open(report_path) as f:
        report = load(f, Loader=SafeLoader)
        start_time = datetime.strptime(report['Start Time'], '%a %b %d %H:%M:%S %Y')
        report_hosts = list(report['Hosts'])
        active_node_changes.append((start_time, len(report_hosts)))
        for host in report_hosts:
            job_length = pd.to_timedelta(report['Hosts'][host]['Application Totals']['runtime (s)'], unit='s')
            # One of the hosts in this report finishes at this time
            active_node_changes.append((start_time + job_length, -1))
    for host in report_hosts:
        trace_path = report_path.rsplit('.report', 1)[0] + f'.trace-{host}'
        df = pd.read_csv(trace_path, comment='#', na_values=['NAN', 'nan'], sep='|')
        df['TIMESTAMP'] = start_time + pd.to_timedelta(df['TIME'], unit='s')
        host_traces.append(df.set_index('TIMESTAMP').resample('20ms')[['CPU_POWER', 'CPU_POWER_LIMIT_CONTROL', 'EPOCH_COUNT']].agg({'CPU_POWER': 'mean', 'CPU_POWER_LIMIT_CONTROL': 'mean', 'EPOCH_COUNT': 'max'}))
        hosts.append(host)

df = pd.concat(host_traces, keys=hosts, names=['Host']).reset_index()
active_nodes_df = pd.DataFrame(active_node_changes, columns=['Time', 'Node Change']).sort_values('Time')
ref_time = active_nodes_df['Time'].min()

host_count = len(set(hosts))
IDLE_WATTS_PER_HOST = 38

df['Time'] = (df['TIMESTAMP'] - ref_time).dt.total_seconds()
df = df.set_index(['Host', 'Time'])
totals_df = df.groupby('Time').sum()
totals_df['Active Nodes'] = df.reset_index().groupby('Time')['Host'].nunique()
totals_df['CPU_POWER'] += (host_count - totals_df['Active Nodes']) * IDLE_WATTS_PER_HOST

active_nodes_df['Active Nodes'] = active_nodes_df['Node Change'].cumsum()
active_nodes_df['Time'] = (active_nodes_df['Time'] - ref_time).dt.total_seconds()

epoch_times = df.reset_index().groupby('EPOCH_COUNT')['Time'].max().dropna()

sns.set()
fig, (ax_total, ax_parts, ax_nodes) = plt.subplots(
    3,
    figsize=(12, 8),
    sharex='col',
    gridspec_kw={'height_ratios': [1, 1, 0.5]})
sns.lineplot(
    ax=ax_total,
    data=pd.melt(totals_df, ignore_index=False, var_name='Signal',
                 value_vars=['CPU_POWER', 'CPU_POWER_LIMIT_CONTROL']).reset_index(),
    x='Time',
    y='value',
    style='Signal')
ax_total.set_ylabel('Total Power')

sns.lineplot(
    ax=ax_parts,
    data=pd.melt(df, ignore_index=False, var_name='Signal',
                 value_vars=['CPU_POWER', 'CPU_POWER_LIMIT_CONTROL']).reset_index(),
    x='Time',
    y='value',
    hue='Host',
    style='Signal')
ax_parts.set_ylabel('Host Power')

ax_nodes.step(x=active_nodes_df['Time'].values, y=active_nodes_df['Active Nodes'].values, where='post')
#for time in epoch_times.to_list():
#    ax_nodes.axvline(time, linestyle='--')

ax_nodes.set_xlabel('Time (s)')
ax_nodes.set_ylabel('Active Nodes')

fig.savefig(args.plot_path, bbox_inches='tight')
