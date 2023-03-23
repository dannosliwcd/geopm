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
parser.add_argument('server_trace')
parser.add_argument('--plot-path', default='cluster_power.png')
parser.add_argument('--mean-target', type=float)
parser.add_argument('--reserve', type=float)

args = parser.parse_args()

active_node_changes = list()
host_traces = list()
hosts = list()

# Reshape to give binned timestamp

df = pd.read_csv(args.server_trace, na_values=['nan'], sep=',', parse_dates=['timestamp'])
ref_time = df['timestamp'].min()
df['Time'] = (df['timestamp'] - ref_time).dt.total_seconds()

sns.set()
fig, ax = plt.subplots(figsize=(12, 8))
if args.mean_target is not None:
    ax.axhline(args.mean_target, linestyle='--', label='Mean Target Power')
    if args.reserve is not None:
        ax.axhspan(args.mean_target - args.reserve, args.mean_target + args.reserve, alpha=0.5, label='Reserve Target Range')
df.set_index('Time')[['target', 'cap', 'measured']].plot(ax=ax, legend=False)
# sns.lineplot(
#     ax=ax,
#     data=pd.melt(totals_df, ignore_index=False, var_name='Signal',
#                  value_vars=['CPU_POWER', 'CPU_POWER_LIMIT_CONTROL']).reset_index(),
#     x='Time',
#     y='value',
#     style='Signal')
ax.legend()
ax.set_ylabel('Power (W)')
ax.set_xlabel('Time (s)')
ax.set_xlim(0, 700)
ax.set_ylim(1000, 2300)
fig.savefig(args.plot_path, bbox_inches='tight')
