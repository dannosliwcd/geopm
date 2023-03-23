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
parser.add_argument('--sharing-reports', nargs='+')
parser.add_argument('--no-sharing-reports', nargs='+')
parser.add_argument('--plot-path', default='power.png')

args = parser.parse_args()


def load_report(report_path):
    total_time = 0
    with open(report_path) as f:
        report = load(f, Loader=SafeLoader)
    report_hosts = list(report['Hosts'])
    profile = report['Profile']
    app = profile.rsplit('/', 1)[-1]
    for host in report_hosts:
        total_time = max(total_time, report['Hosts'][host]['Application Totals']['runtime (s)'])
    return {'Application': app, 'Time': total_time}


sharing_reports = list()
no_sharing_reports = list()
for report_path in args.sharing_reports:
    sharing_reports.append(load_report(report_path))
sharing_df = pd.DataFrame(sharing_reports)
sharing_df['Policy'] = 'Sharing'

for report_path in args.no_sharing_reports:
    no_sharing_reports.append(load_report(report_path))
no_sharing_df = pd.DataFrame(no_sharing_reports)
no_sharing_df['Policy'] = 'No Sharing'

df = pd.concat((sharing_df, no_sharing_df))

sns.set()
fig, ax = plt.subplots(figsize=(12, 10))
sns.barplot(
    ax=ax,
    data=df,
    x='Application',
    y='Time',
    hue='Policy',
)
fig.savefig(args.plot_path, bbox_inches='tight')
