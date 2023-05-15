#!/usr/bin/env python3
import argparse
from datetime import datetime
from yaml import load
from matplotlib import pyplot as plt
import matplotlib.ticker as mtick
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
parser.add_argument('--reserve-only', action='store_true')
parser.add_argument('--max-time', type=float, default=3600)

args = parser.parse_args()

active_node_changes = list()
host_traces = list()
hosts = list()

# Reshape to give binned timestamp

df = pd.read_csv(args.server_trace, na_values=['nan'], sep=',', parse_dates=['timestamp'])
ref_time = df['timestamp'].min()
df['Time'] = (df['timestamp'] - ref_time).dt.total_seconds()

FONT_SIZE = 8.0
sns.set_theme(
    context='paper',
    style='whitegrid',
    palette='colorblind',
    rc={'font.size': FONT_SIZE,
        'axes.titlesize': FONT_SIZE,
        'axes.labelsize': FONT_SIZE,
        'xtick.labelsize': FONT_SIZE-1,
        'ytick.labelsize': FONT_SIZE-1,
        'legend.fontsize': FONT_SIZE,
        }
)
fig, ax = plt.subplots(figsize=(3.35, 2.0))
if args.mean_target is not None:
    ax.axhline(args.mean_target, linestyle='--', label='Mean Target Power')
    if args.reserve is not None and not args.reserve_only:
        ax.axhspan(args.mean_target - args.reserve, args.mean_target + args.reserve, alpha=0.5, label='Reserve Target Range')
df.rename(columns={'target': 'Target', 'cap': 'Cap', 'measured': 'Measured'}, inplace=True)
df.set_index('Time')[['Target', 'Cap', 'Measured']].plot(ax=ax, legend=False)
# sns.lineplot(
#     ax=ax,
#     data=pd.melt(totals_df, ignore_index=False, var_name='Signal',
#                  value_vars=['CPU_POWER', 'CPU_POWER_LIMIT_CONTROL']).reset_index(),
#     x='Time',
#     y='value',
#     style='Signal')
ax.legend(loc='lower center', bbox_to_anchor=(0.5, 1), ncol=2)
ax.set_ylabel('Power (W)')
ax.set_xlabel('Time (s)')
ax.set_xlim(0, args.max_time)
#ax.xaxis.set_major_locator(ticker.LinearLocator(4))
if args.reserve is not None and args.mean_target is not None and args.reserve_only:
    ax.set_ylim(args.mean_target - args.reserve, args.mean_target + args.reserve)
fig.savefig(args.plot_path, bbox_inches='tight')
