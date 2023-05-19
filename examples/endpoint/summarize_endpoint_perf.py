#!/usr/bin/env python3
import argparse
from datetime import datetime
from yaml import load
from matplotlib import pyplot as plt
import pandas as pd
import matplotlib.ticker as mtick
import seaborn as sns
import glob
import os
from sklearn import linear_model
from sklearn.preprocessing import PolynomialFeatures
import numpy as np
try:
    from yaml import CSafeLoader as SafeLoader
except ImportError:
    from yaml import SafeLoader

parser = argparse.ArgumentParser('Generate a summary of an endpoint experiment')
parser.add_argument('report_paths', nargs='+',
                    help='<name>:<path> pairs of names and paths to directories containing reports')
#parser.add_argument('--sharing-report-paths', nargs='+')
#parser.add_argument('--non-sharing-report-paths', nargs='+')
parser.add_argument('--plot-path', default='endpoint_apps.png')
parser.add_argument('--max-time', type=float, default=3600)
parser.add_argument('--reference-experiment')
parser.add_argument('--separate-legend', action='store_true')
parser.add_argument('--app-info',
                    default='app_properties.yaml',
                    help='Path to application characterization data')

args = parser.parse_args()

# Reshape to give binned timestamp


def load_job_summaries(paths):
    job_summaries = list()
    for report_path in paths:
        with open(report_path) as f:
            report = load(f, Loader=SafeLoader)
            start_time = datetime.strptime(report['Start Time'], '%a %b %d %H:%M:%S %Y')
            report_hosts = list(report['Hosts'])
            profile = report['Profile']
            app = profile.rsplit('/', 1)[-1]
            job_length = max(host_data['Application Totals']['runtime (s)'] for host_data in report['Hosts'].values())
            host_power = sum(host_data['Application Totals']['power (W)'] for host_data in report['Hosts'].values())
            total_energy = (sum(host_data['Application Totals']['package-energy (J)'] for host_data in report['Hosts'].values()) +
                            sum(host_data['Application Totals']['dram-energy (J)'] for host_data in report['Hosts'].values()))
            hosts = len(report['Hosts'])
            job_summaries.append({'app': app, 'start_timestamp': start_time,
                                  'time': job_length, 'power': host_power,
                                  'hosts': hosts, 'energy': total_energy})
    return pd.DataFrame(job_summaries)


dfs = list()
reference_times_by_app = None

experiment_order = list()
for report_path_spec in args.report_paths:
    experiment_name, report_dir = report_path_spec.split(':', 1)
    experiment_order.append(experiment_name)
    df = load_job_summaries(glob.glob(os.path.join(report_dir, '*.report')))
    df['Power Budgeter'] = experiment_name
    earliest_time = df['start_timestamp'].min()
    df['start time'] = (df['start_timestamp'] - earliest_time).dt.total_seconds()
    dfs.append(df)

# if args.sharing_report_paths is not None:
#     df_sharing = load_job_summaries(args.sharing_report_paths)
#     df_sharing['Power Budgeter'] = 'Perf-Sharing'
#     earliest_time = df_sharing['start_timestamp'].min()
#     df_sharing['start time'] = (df_sharing['start_timestamp'] - earliest_time).dt.total_seconds()
#     dfs.append(df_sharing)
# 
# if args.non_sharing_report_paths is not None:
#     df_no_sharing = load_job_summaries(args.non_sharing_report_paths)
#     df_no_sharing['Power Budgeter'] = 'Power-Sharing'
#     earliest_time = df_no_sharing['start_timestamp'].min()
#     df_no_sharing['start time'] = (df_no_sharing['start_timestamp'] - earliest_time).dt.total_seconds()
#     dfs.append(df_no_sharing)
df = pd.concat(dfs).reset_index()
df['end time'] = df['start time'] + df['time']
df = df.loc[df['end time'] <= args.max_time]

with open(args.app_info) as f:
    app_info = load(f, Loader=SafeLoader)['applications']
    reference_times_by_app = {app: app_data['min_time'] for app, app_data in app_info.items()}
if args.reference_experiment is not None:
    reference_times_by_app = df.loc[df['Power Budgeter'] == args.reference_experiment].groupby('app')['time'].mean().to_dict()

if reference_times_by_app is not None:
    df['reltime'] = df['time'] / df['app'].map(reference_times_by_app)
    df['slowdown'] = df['reltime'] - 1
reference_energy_by_app = df.groupby('app')['energy'].min().to_dict()
df['relenergy'] = df['energy'] / df['app'].map(reference_energy_by_app)
df['mean power'] = df['power'] / df['hosts']

started_hosts = df.pivot(['Power Budgeter', 'index', 'start time'], 'app', 'hosts').fillna(0)
started_hosts.index.rename('time', 'start time', inplace=True)
finished_hosts = (-df.pivot(['Power Budgeter', 'index', 'end time'], 'app', 'hosts').fillna(0))
finished_hosts.index.rename('time', 'end time', inplace=True)
active_hosts_df = pd.concat((started_hosts, finished_hosts)).groupby('Power Budgeter', as_index=False).apply(lambda x: x.sort_values('time').cumsum())
# active_sharing_hosts_df = active_hosts_df.droplevel([0, 2]).loc['Perf-Sharing', slice(None)]

path, ext = args.plot_path.rsplit('.', 1)

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

# ========= power =======
fig, ax = plt.subplots(figsize=(3.35, 2.0))
sns.pointplot(
    ax=ax,
    data=df,
    x='slowdown',
    dodge=True,
    y='app',
    hue='Power Budgeter',
    #showfliers=False,
    #cut=0,
)
reference_name = args.reference_experiment if args.reference_experiment is not None else 'No Power Cap'
ax.set_xlabel(f'Slowdown vs. {reference_name}')
ax.set_ylabel('Job Type')
ax.xaxis.set_major_formatter(mtick.PercentFormatter(1,0))
if args.separate_legend:
    legend = ax.get_legend_handles_labels()
    ax.get_legend().remove()
    figlegend = plt.figure(figsize=(3.35, 1))
    figlegend.legend(legend[0], legend[1], ncol=len(experiment_order))
    figlegend.savefig(f'{path}-legend.{ext}', bbox_inches='tight', pad_inches=0.05)
fig.savefig(f'{path}.{ext}', bbox_inches='tight', pad_inches=0)
print('====Mean slowdown====') 
#print(df.loc[df['app'] != 'mg.D.x'].groupby('Power Budgeter')['slowdown'].mean().to_string())
print(df.groupby('Power Budgeter')['slowdown'].mean().to_string())
print('====Mean energy====') 
print(df.groupby('Power Budgeter')['energy'].mean().to_string())
print('====Jobs completed====') 
print(df.groupby('Power Budgeter').size().to_string())

fig, ax = plt.subplots(figsize=(3.35, 2.0))
sns.pointplot(
    ax=ax,
    data=df,
    x='mean power',
    dodge=True,
    y='app',
    hue='Power Budgeter',
    #showfliers=False,
    #cut=0,
)
ax.set_xlabel('Power Per Node')
ax.set_ylabel('Job Type')
sns.move_legend(ax, 'upper left')
if args.separate_legend:
    ax.get_legend().remove()
fig.savefig(f'{path}-power.{ext}', bbox_inches='tight', pad_inches=0)

# ========= Scheduled Hosts =======
# fig, ax = plt.subplots(figsize=(7, 3.5))
# active_sharing_hosts_df.plot.area(ax=ax, linewidth=0)
# ax.set_ylabel('Active Hosts')
# ax.set_xlabel('Time (seconds since schedule start)')
# fig.savefig(f'{path}-schedule.{ext}', bbox_inches='tight', pad_inches=0)

# ========= Schedule Energy =======

# ========= General Slowdown Comparison =======
# sns.kdeplot(
#     ax=ax,
#     data=df,
#     x='reltime',
#     hue='Power Budgeter',
#     cut=0,
# )

#sns.move_legend(ax, 'lower left')
#ax.set_xlabel('$90^{th}$ Percentile Slowdown')
#ax.set_ylabel('App')

print('===== time x hosts =====')
print(df.groupby('Power Budgeter').apply(lambda x: (x['time'] * x['hosts']).mean()).to_string())
print('===== reltime =====')
print(df.groupby(['Power Budgeter', 'app'])['reltime'].mean().to_string())
print('===== time =====')
print(df.groupby(['Power Budgeter', 'app'])['time'].mean().to_string())

