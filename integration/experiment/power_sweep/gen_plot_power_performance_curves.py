#!/usr/bin/env python3
#
#  Copyright (c) 2015 - 2022, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

'''
Generate power-performance curves of applications in power sweeps.
'''

import os
import pandas as pd
import argparse
import matplotlib.pyplot as plt
import itertools
import seaborn as sns
import glob

from experiment import common_args
from experiment import plotting

from yaml import load
try:
    from yaml import CSafeLoader as SafeLoader
except ImportError:
    from yaml import SafeLoader


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    common_args.add_output_dir(parser)
    common_args.add_analysis_dir(parser)
    common_args.add_label(parser)
    parser.add_argument('--plot-suffix', default='png',
                        help='File suffix to use for plots')
    parser.add_argument('--try-epochs', action='store_true',
                        help='Whether to use epoch totals when available (more than 0 epochs). Default: only use app totals')

    args = parser.parse_args()
    output_dir = args.output_dir

    report_paths = list()
    for dir_path, dir_names, file_names, in os.walk(output_dir):
        for file_name in file_names:
            if file_name.endswith('.report'):
                report_paths.append(os.path.join(dir_path, file_name))

    report_stats = list()
    for report_path in report_paths:
        with open(report_path) as f:
            report = load(f, Loader=SafeLoader)

        report_hosts = list(report['Hosts'])
        power_cap = report['Policy'].get('CPU_POWER_LIMIT')
        profile = report['Profile']
        agent = report['Agent']
        # strip path components from the app name if they exist
        app = profile.rsplit('/', 1)[-1]
        # strip sweep descriptors from the app name if they exist
        agent_name_idx = app.find(f'_{agent}_')
        if agent_name_idx != -1:
            app = app[:agent_name_idx]
        time = max(host_data['Application Totals']['runtime (s)']
                   for host_data in report['Hosts'].values())
        report_stats.append({
            'Application': app,
            'Power Cap (W)': power_cap,
            'Time (s)': time,
        })
    df = pd.DataFrame(report_stats)
    ref_time_by_app  = df.groupby('Application').apply(
        lambda x: x.loc[x['Power Cap (W)'] == x['Power Cap (W)'].max(), 'Time (s)'].mean())
    df['Relative Time'] = df['Time (s)'] / df['Application'].map(ref_time_by_app)

    plt.rcParams.update({'font.size': 8.0})
    fig, ax = plt.subplots(figsize=(3.35, 2.8))
    markers = itertools.cycle((',', '.', '*', '^', 'v', '>'))
    for app_name, app_data in df.groupby('Application'):
        plot_data = app_data.groupby('Power Cap (W)')['Relative Time'].agg(['mean', 'std'])
        print(app_name)
        print(plot_data.to_string())
        print('--------')
        ax.errorbar(plot_data.index, plot_data['mean'],
                    yerr=plot_data['std'],
                    marker=next(markers),
                    label=app_name)
    fig.subplots_adjust(bottom=0.25)
    fig.legend(title='Application', ncol=3,
               loc='upper center',
               columnspacing=0.5,
               fontsize=8,
               bbox_transform=fig.transFigure,
               bbox_to_anchor=(0.5, 0.1))
    ax.set_xlabel('Power Cap (W)')
    ax.set_ylabel('Time (relative to uncapped)')

    file_name = f'{args.label}_time_comparison.{args.plot_suffix}'
    os.makedirs(args.analysis_dir, exist_ok=True)
    full_path = os.path.join(args.analysis_dir, file_name)
    fig.savefig(full_path, bbox_inches='tight', pad_inches=0)
