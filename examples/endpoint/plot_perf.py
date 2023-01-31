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

parser = argparse.ArgumentParser('Generate a power plot')
parser.add_argument('report_paths', nargs='+')
parser.add_argument('--plot-path', default='power.png')

args = parser.parse_args()

active_node_changes = list()
epoch_metrics = list()
reports = list()

# Reshape to give binned timestamp

for report_path in args.report_paths:
    with open(report_path) as f:
        report = load(f, Loader=SafeLoader)
        start_time = datetime.strptime(report['Start Time'], '%a %b %d %H:%M:%S %Y')
        report_hosts = list(report['Hosts'])
        profile = report['Profile']
        app = profile.rsplit('/', 1)[-1]
        active_node_changes.append((start_time, len(report_hosts)))
        for host in report_hosts:
            job_length = pd.to_timedelta(report['Hosts'][host]['Application Totals']['runtime (s)'], unit='s')
            # One of the hosts in this report finishes at this time
            active_node_changes.append((start_time + job_length, -1))

    report_host_dfs = list()
    for host in report_hosts:
        trace_path = report_path.rsplit('.report', 1)[0] + f'.trace-{host}'
        df = pd.read_csv(trace_path, comment='#', na_values=['NAN', 'nan'], sep='|')

        cap_by_epoch = df.groupby('EPOCH_COUNT')['CPU_POWER_LIMIT_CONTROL'].mean()
        time_by_epoch = df.groupby('EPOCH_COUNT')['TIME'].last().diff()
        df = pd.concat([time_by_epoch, cap_by_epoch], axis=1).dropna()
        df['App'] = app
        reftime = df.loc[df['CPU_POWER_LIMIT_CONTROL'] > df['CPU_POWER_LIMIT_CONTROL'].quantile(0.8), 'TIME'].quantile(0.1)
        df['RelTime'] = df['TIME'] / reftime
        report_host_dfs.append(df)

    # Aggregate per-host data to a single value per report. This is meant to
    # emulate how the endpoint would see a single value per job instead of
    # per-host data.
    df = pd.concat(report_host_dfs, keys=report_hosts, names=['Host']).reset_index()
    df = df.groupby('EPOCH_COUNT').agg({
        'CPU_POWER_LIMIT_CONTROL': 'mean',
        'TIME': 'max', 'App': 'first', 'RelTime': 'max'}).reset_index()

    train_df = df

    poly = PolynomialFeatures(2, include_bias=False)
    def is_model_valid(model, X, y):
        # Check that time estimates are monotonically decreasing as we increase power
        times = model.predict(poly.fit_transform(np.linspace(0, 280, 8).reshape(-1, 1)))
        return np.all(times[:-1] >= times[1:])

    # Probably small enough epoch counts that Theil-Sen is good enough.
    reg = linear_model.RANSACRegressor(is_model_valid=is_model_valid)
    # TODO: ValueError if RANSAC cannot fit. Then fall back to heuristics
    reg.fit(poly.fit_transform(train_df[['CPU_POWER_LIMIT_CONTROL']]), train_df['TIME'])
    df['TIME_PRED'] = reg.predict(poly.fit_transform(df[['CPU_POWER_LIMIT_CONTROL']]))

    epoch_metrics.append(df)
    reports.append(report_path)

df = pd.concat(epoch_metrics, keys=reports, names=['Report']).reset_index()
df['Run'] = df['Report'] + '(' + df['App'] + ')'

sns.set()
fig, (ax_curve, ax_dist) = plt.subplots(2, figsize=(12, 10), sharex=True,
                                        gridspec_kw={'height_ratios': [1, 0.25]})
sns.scatterplot(
    ax=ax_curve,
    data=df,
    x='CPU_POWER_LIMIT_CONTROL',
    y='RelTime',
    hue='App',
    style='Report',
)
#  sns.lineplot(
#      ax=ax_curve,
#      data=df,
#      x='CPU_POWER_LIMIT_CONTROL',
#      y='TIME_PRED',
#      hue='App',
#      units='Report',
#      estimator=None,
#      legend=False,
#  )

sns.kdeplot(
    ax=ax_dist,
    data=df,
    x='CPU_POWER_LIMIT_CONTROL',
    hue='App',
    cut=0,
    common_norm=False,
)
ax_curve.axhline(1.05, linestyle='--')
ax_curve.set_ylim((0.25, 3.75))
ax_dist.set_xlim((130, 290))
fig.savefig(args.plot_path, bbox_inches='tight')
