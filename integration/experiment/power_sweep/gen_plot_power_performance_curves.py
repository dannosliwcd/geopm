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
import numpy as np
import argparse
import matplotlib.pyplot as plt
import itertools
import seaborn as sns
import glob

import sklearn
from sklearn import linear_model
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import PolynomialFeatures, FunctionTransformer

from experiment import common_args
from experiment import plotting

from yaml import load
try:
    from yaml import CSafeLoader as SafeLoader
except ImportError:
    from yaml import SafeLoader

def make_model():
    pipeline = Pipeline([
        ("transform_center", FunctionTransformer(lambda x: (280-x)**2)),
        #("polynomial_features", PolynomialFeatures(2, include_bias=False)),
        ("linear_regression", linear_model.LinearRegression(positive=True)),
    ])
    return pipeline

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
        epoch_time = max(host_data['Epoch Totals']['runtime (s)']
                         for host_data in report['Hosts'].values())
        epoch_count = max(host_data['Epoch Totals']['count']
                          for host_data in report['Hosts'].values())
        report_stats.append({
            'Application': app,
            'Power Cap (W)': power_cap,
            'Time (s)': time,
            'Epoch Time (s)': epoch_time,
            'Epoch Count': epoch_count,
        })
    df = pd.DataFrame(report_stats)
    ref_time_by_app  = df.groupby('Application').apply(
        lambda x: x.loc[x['Power Cap (W)'] == x['Power Cap (W)'].max(), 'Time (s)'].mean())
    df['Relative Time'] = df['Time (s)'] / df['Application'].map(ref_time_by_app)

    FONT_SIZE = 8
    sns.set_theme(
        context='paper',
        style='whitegrid',
        rc={'font.size': FONT_SIZE,
            'axes.titlesize': FONT_SIZE,
            'axes.labelsize': FONT_SIZE,
            'xtick.labelsize': FONT_SIZE-1,
            'ytick.labelsize': FONT_SIZE-1,
            'legend.fontsize': FONT_SIZE,
            }
    )
    plt.rcParams.update({'font.size': 8.0})
    fig, ax = plt.subplots(figsize=(3.35, 2.5))
    markers = itertools.cycle((',', '.', '*', '^', 'v', '>'))
    for app_name, app_data in df.groupby('Application'):
        plot_data = app_data.groupby('Power Cap (W)')['Relative Time'].agg(['mean', 'std'])
        print(app_name)
        print(plot_data.to_string())
        ax.errorbar(plot_data.index, plot_data['mean'],
                    yerr=plot_data['std'],
                    marker=next(markers),
                    label=app_name)

        model = make_model()
        X = app_data['Power Cap (W)'].values.reshape(-1, 1)
        y = app_data['Time (s)']
        model.fit(X, y)
        y_train_pred = model.predict(X)

        plotx = np.linspace(app_data['Power Cap (W)'].min(), app_data['Power Cap (W)'].max(), 10).reshape(-1, 1)
        ploty = model.predict(plotx)/model.predict(np.array(app_data['Power Cap (W)'].max()).reshape(-1, 1))
        #ax.plot(plotx, ploty, c='k', lw=0.25)

        r2score = sklearn.metrics.r2_score(y, y_train_pred)
        print('Totals Training R2 Score:', r2score)
        print(f'Totals A: {model.named_steps["linear_regression"].coef_}, C: {model.named_steps["linear_regression"].intercept_}')

        model = make_model()
        X = app_data['Power Cap (W)'].values.reshape(-1, 1)
        y = app_data['Epoch Time (s)'] / app_data['Epoch Count']
        if y.isna().sum() == 0:
            model.fit(X, y)
            y_train_pred = model.predict(X)
            r2score = sklearn.metrics.r2_score(y, y_train_pred)
            print('Epoch Training R2 Score:', r2score)
            print(f'Epoch A: {model.named_steps["linear_regression"].coef_}, C: {model.named_steps["linear_regression"].intercept_}')
        print('--------')

    print('Average slowdown by app')
    print(df.groupby('Application')['Relative Time'].mean().sort_values().to_string())


    fig.subplots_adjust(bottom=0.23)
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
