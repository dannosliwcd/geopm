#!/usr/bin/env python3
import argparse
from datetime import datetime
from yaml import load
from matplotlib import pyplot as plt
import pandas as pd
import seaborn as sns
from sklearn import linear_model
from sklearn.preprocessing import PolynomialFeatures
import numpy as np
try:
    from yaml import CSafeLoader as SafeLoader
except ImportError:
    from yaml import SafeLoader

parser = argparse.ArgumentParser('Generate a summary of an endpoint experiment')
parser.add_argument('--perf-sharing-cluster-trace')
parser.add_argument('--power-sharing-cluster-trace')
parser.add_argument('--non-sharing-cluster-trace')
parser.add_argument('--reserve', type=float, required=True)
parser.add_argument('--plot-path', default='endpoint_error.png')

args = parser.parse_args()

# Reshape to give binned timestamp


dfs = list()
if args.perf_sharing_cluster_trace is not None:
    df = pd.read_csv(args.perf_sharing_cluster_trace)
    df['Method'] = 'Perf-Sharing'
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    earliest_time = df['timestamp'].min()
    df['time'] = (df['timestamp'] - earliest_time).dt.total_seconds()
    dfs.append(df)
if args.power_sharing_cluster_trace is not None:
    df = pd.read_csv(args.power_sharing_cluster_trace)
    df['Method'] = 'Power-Sharing'
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    earliest_time = df['timestamp'].min()
    df['time'] = (df['timestamp'] - earliest_time).dt.total_seconds()
    dfs.append(df)
if args.non_sharing_cluster_trace is not None:
    df = pd.read_csv(args.non_sharing_cluster_trace)
    df['Method'] = 'Non-Sharing'
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    earliest_time = df['timestamp'].min()
    df['time'] = (df['timestamp'] - earliest_time).dt.total_seconds()
    dfs.append(df)
df = pd.concat(dfs).reset_index()
df['error'] = ((df['target'] - df['measured']) / args.reserve).abs()

sns.set_theme(
    context='paper',
    style='whitegrid',
    rc={'font.size': 8.0}
)
fig, ax = plt.subplots(figsize=(3.35, 1.8))
#ax.axhline(0.9, linestyle='--', label='Target')
#ax.axvline(0.3, linestyle='--')
sns.ecdfplot(
    ax=ax,
    data=df.loc[(df['time'] > 0) & (df['time'] <= 3600)],
    x='error',
    hue='Method',
    #cut=0,
    #common_norm=False,
)
sns.move_legend(ax, 'lower right')
ax.set_xlabel('Power Tracking Error')
fig.savefig(args.plot_path, bbox_inches='tight', pad_inches=0)

print('90%ile: ', df.loc[(df['time'] > 0) & (df['time'] <= 3600)].groupby('Method')['error'].quantile(0.9))
print('mean: ', df.loc[(df['time'] > 0) & (df['time'] <= 3600)].groupby('Method')['error'].mean(0.9))
