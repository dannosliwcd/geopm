#!/usr/bin/env python3
import argparse
from datetime import datetime
import matplotlib.ticker as mtick
from yaml import load
from matplotlib import pyplot as plt
import os
import glob
import pandas as pd
import seaborn as sns
from sklearn import linear_model
from sklearn.preprocessing import PolynomialFeatures
import numpy as np
try:
    from yaml import CSafeLoader as SafeLoader
except ImportError:
    from yaml import SafeLoader

parser = argparse.ArgumentParser()
parser.add_argument('report_paths', nargs='+',
                    help='<name>:<path> pairs of names and paths to directories containing reports')
parser.add_argument('--plot-true-job-types', action='store_true',
                    help='Plot a bar for each true job type instead of separate ones for each mischaracterization')
parser.add_argument('--skip-partial-jobs', action='store_true',
                    help='Do not plot jobs that end after a co-scheduled job type has completely finished running')
parser.add_argument('--app-info',
                    default='app_properties.yaml',
                    help='Path to application characterization data')
parser.add_argument('--reference-times', nargs='+',
                    help='<job_type>:<time_seconds> pairs of job types and times to normalize against')
parser.add_argument('--plot-path', default='slowdown.png')
# Example:
# ~/geopm/examples/endpoint/plot_cluster_policies.py --plot-path ~/public_html/cluster_btep.png 'No Perf Awareness':endpoint_btep_noperf 'All Known Perf':endpoint_btep_prechar_nolearn 'Under-estimate bt':endpoint_btep_bteqis_nolearn 'Under-estimate bt, with feedback':endpoint_btep_bteqis_learn 'Under-estimate ep':endpoint_btep_epeqis_nolearn 'Under-estimate ep, with feedback':endpoint_btep_epeqis_learn --reference-times 'ep.D.x=36.957' 'is.D.x=25' 'sp.D.x=584' 'bt.D.x=371'
#
#~/geopm/examples/endpoint/plot_cluster_policies.py --plot-path ~/public_html/cluster_btsp.pdf 'No Perf Awareness':endpoint_btsp_noshare 'All Known Perf':endpoint_btsp_prechar_nolearn 'Under-estimate bt':endpoint_btsp_bteqis_nolearn 'Under-estimate bt, with feedback':endpoint_btsp_bteqis_learn 'Over-estimate sp':endpoint_btsp_speqep_nolearn 'Over-estimate sp, with feedback':endpoint_btsp_speqep_learn --reference-times 'ep.D.x=36.957' 'is.D.x=25' 'sp.D.x=584' 'bt.D.x=371'
#
#~/geopm/examples/endpoint/plot_cluster_policies.py --plot-path ~/public_html/cluster_issp.pdf 'No Perf Awareness':endpoint_issp_noshare 'All Known Perf':endpoint_issp_prechar_nolearn 'Over-estimate is':endpoint_issp_iseqep_nolearn 'Over-estimate is, with feedback':endpoint_issp_iseqep_learn 'Over-estimate sp':endpoint_issp_speqep_nolearn 'Over-estimate sp, with feedback':endpoint_issp_speqep_learn --reference-times 'ep.D.x=36.957' 'is.D.x=25' 'sp.D.x=584' 'bt.D.x=371'

args = parser.parse_args()

# with open(args.app_info) as f:
#     app_info = load(f, Loader=SafeLoader)
# applications:
#   bt.D.x:
#     model:
#       A: 5.65000985e-05
#       C: 1.4587099558620689
#     nodes: 2
#     min_time: 368
#     launcher: './launch_bt.d.sh'

report_data = list()
experiment_order = list()
for report_path_spec in args.report_paths:
    experiment_name, report_dir = report_path_spec.split(':', 1)
    experiment_order.append(experiment_name)
    for report_path in glob.iglob(os.path.join(report_dir, '*.report')):
        with open(report_path) as f:
            report = load(f, Loader=SafeLoader)
            profile = report['Profile']
            job_type = profile.rsplit('/', 1)[-1]
            if args.plot_true_job_types:
                if '=' in job_type:
                    job_type = job_type.split('=', 1)[0]
            start_time = datetime.strptime(report['Start Time'], '%a %b %d %H:%M:%S %Y')
            execution_time = max(host_data['Application Totals']['runtime (s)']
                                 for host_data in report['Hosts'].values())
            report_data.append({'Experiment Name': experiment_name,
                                'Job Type': job_type,
                                'Execution Time': execution_time,
                                'Start Timestamp': start_time,
                                'End Timestamp': start_time + pd.to_timedelta(execution_time, unit='s'),
                                })
df = pd.DataFrame(report_data)

# Stop the experiment when one job type finishes its final run. The co-scheduled
# job type will be given surplus power from that point on which positively biases 
# its performance, so ignore jobs that finish after that point.
if args.skip_partial_jobs:
    experiment_end_times = df.groupby(['Experiment Name', 'Job Type'])['End Timestamp'].max().groupby('Experiment Name').min()
    is_job_fully_in_experiment = (df['End Timestamp'] <= df['Experiment Name'].map(experiment_end_times))
    df = df.loc[is_job_fully_in_experiment]

if args.reference_times is not None:
    reference_times = dict()
    for spec in args.reference_times:
        name, value = spec.rsplit('=', 1)
        reference_times[name] = float(value)
    df['Slowdown'] = df['Execution Time'] / df['Job Type'].map(reference_times) - 1

sns.set_theme(
    context='paper',
    style='whitegrid',
    rc={'font.size': 8.0}
)
fig, ax = plt.subplots(figsize=(3.35, 3.0))
sns.barplot(
    ax=ax,
    data=df,
    ci='sd',
    x='Slowdown' if 'Slowdown' in df.columns else 'Execution Time',
    y='Experiment Name',
    hue='Job Type',
    order=experiment_order,
)
print(df.groupby(['Job Type', 'Experiment Name'])[['Slowdown']].mean().to_string())
print(df.groupby(['Job Type', 'Experiment Name'])[['Execution Time']].mean().to_string())
ax.xaxis.set_major_formatter(mtick.PercentFormatter(1))
ax.set_ylabel('')
fig.savefig(args.plot_path, bbox_inches='tight', pad_inches=0)
