#!/usr/bin/env python3
import argparse
import numpy as np
import pandas as pd
import sys
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
import seaborn as sns
from scipy import optimize
from yaml import load
try:
    from yaml import CSafeLoader as SafeLoader
except ImportError:
    from yaml import SafeLoader


# Not used here.
def time_multiplier_at_power(x, A, B, C):
    """Return time multiplier at a given power cap"""
    return (A * (280-x)**2 + B * (280-x) + C) / C


def power_at_time_multiplier(time_multiplier, A, B, C):
    """Return power consumed at a given target time multiplier"""
    return np.clip(280 - (-B + np.sqrt(B**2 - 4 * A * C * (1 - time_multiplier))) / (2 * A), 140, 280)


def power_deficit_at_time_multiplier(time_multiplier, budget, A, B, C, nodes_per_job):
    """Return the power deficit at a target uniform slowdown across jobs for a
    given power budget. I.e., the zero-crossing point is where the budget is
    allocated exactly, and where all jobs have the same expected slowdown.
    """
    # TODO: chain rule: [f(g(x))]' = f'(g(x)) * g'(x)
    return (nodes_per_job * power_at_time_multiplier(time_multiplier, A, B, C)).sum() - budget


def get_time_multiplier_and_caps_at_budget(budget, A, B, C, nodes_per_job):
    time_multiplier, root_results = optimize.toms748(
        power_deficit_at_time_multiplier,
        a=1.0,
        b=10.0,
        args=(budget, A, B, C, nodes_per_job),
        full_output=True)
    #print(f'Solution found in {root_results.iterations} iterations, and {root_results.function_calls} function calls')
    selected_caps = power_at_time_multiplier(time_multiplier * np.ones(len(running_jobs)), A, B, C)
    return time_multiplier, selected_caps


def evaluate_jobs(budget, running_jobs, job_model_overrides=None):
    if job_model_overrides is None:
        job_model_overrides = dict()
    running_jobs['A1'] = running_jobs['A']
    running_jobs['B1'] = running_jobs['B']
    running_jobs['C1'] = running_jobs['C']
    for actual_job, modeled_job in job_model_overrides.items():
        model = df.set_index('actual app').loc[[modeled_job]].reset_index()
        actual_job_idx = running_jobs['actual app'] == actual_job
        running_jobs.loc[actual_job_idx, 'modeled app'] = model.iloc[0]['actual app']
        running_jobs.loc[actual_job_idx, 'A1'] = model.iloc[0]['A']
        running_jobs.loc[actual_job_idx, 'B1'] = model.iloc[0]['B']
        running_jobs.loc[actual_job_idx, 'C1'] = model.iloc[0]['C']

    actual_A = running_jobs['A'].values
    actual_B = running_jobs['B'].values
    actual_C = running_jobs['C'].values

    guess_A = running_jobs['A1'].values
    guess_B = running_jobs['B1'].values
    guess_C = running_jobs['C1'].values

    nodes_per_job = running_jobs['nodes'].values

    # Predict time and job caps from what the actual models show for each app
    time_multiplier, selected_caps = get_time_multiplier_and_caps_at_budget(
        budget, actual_A, actual_B, actual_C, nodes_per_job)

    # Predict time and job caps from what the guessed models show for each app
    guess_time_multiplier, guess_caps = get_time_multiplier_and_caps_at_budget(
        budget, guess_A, guess_B, guess_C, nodes_per_job)

    # if args.confuse_job is not None:
    #     job_cap = selected_caps[running_jobs['actual app'] == actual_job]
    #     actual_properties = df.set_index('actual app').loc[actual_job]
    #     actual_time_multiplier = time_multiplier_at_power(job_cap,
    #                                         actual_properties['A'],
    #                                         actual_properties['B'],
    #                                         actual_properties['C'])
    #     print(f'Treating {actual_job} as {modeled_job}. As a result, actual slowdown '
    #           f'of {actual_job} is {actual_time_multiplier}'
    #           )

    # Calculate the expected slowdown when we attempt to balance for even
    # slowdown. This may not be constant across all job types even though that
    # is what we aim to achieve because some job types might be insensitive
    # enough that they have very little slowdown even at the minimum power cap.
    time_multiplier_at_even_time_multiplier_distribution = time_multiplier_at_power(
        selected_caps,
        actual_A, actual_B, actual_C)

    # Calculate the *actual* slowdown that results from the *guess-driven* caps
    time_multiplier_at_guess_distribution = time_multiplier_at_power(
        guess_caps,
        actual_A, actual_B, actual_C)

    even_power_distribution = budget / nodes_per_job.sum()
    time_multiplier_at_even_power_distribution = time_multiplier_at_power(
        np.ones(len(nodes_per_job)) * even_power_distribution,
        actual_A, actual_B, actual_C)

    running_jobs['even-slowdown caps'] = selected_caps
    running_jobs['even-slowdown slowdowns'] = time_multiplier_at_even_time_multiplier_distribution - 1
    running_jobs['even-cap slowdowns'] = time_multiplier_at_even_power_distribution - 1
    running_jobs['guess-cap slowdowns'] = time_multiplier_at_guess_distribution - 1


parser = argparse.ArgumentParser()
parser.add_argument('config_path')
parser.add_argument('--plot-path')
parser.add_argument('--job-types', nargs='+')
parser.add_argument('--show-all-jobs', action='store_true')
parser.add_argument('--show-title', action='store_true')
parser.add_argument('--print-csv', action='store_true')
parser.add_argument('--confuse-jobs', nargs='*', default=[],
                    help='Confuse one job for another. E.g., to use the ep.D.x model '
                         'for bt.D.x, use --confuse-job bt.D.x=ep.D.x')

args = parser.parse_args()

with open(args.config_path) as f:
    app_info = load(f, Loader=SafeLoader)

df = pd.DataFrame(
    {'modeled app': app,
     'actual app': app,
     'A': data['model'].get('A', 0),
     'B': data['model'].get('B', 0),
     'C': data['model'].get('C', 0),
     'nodes': data['nodes']}
    for app, data in app_info['applications'].items())

job_size_overrides = df.set_index('actual app')['nodes'].to_dict()
if args.job_types is None:
    job_types = df['actual app'].unique()
else:
    job_types = list()
    for job_type_and_size in args.job_types:
        if '=' in job_type_and_size:
            job_type, job_size = job_type_and_size.split('=', 1)
            job_types.append(job_type)
            job_size_overrides[job_type] = int(job_size)
        else:
            job_types.append(job_type_and_size)

job_model_overrides = dict()
for confusion in args.confuse_jobs:
    actual_job, modeled_job = confusion.split('=', 1)
    job_model_overrides[actual_job] = modeled_job
running_jobs = df.set_index('actual app').loc[job_types].reset_index().copy()

# TODO: feed in the current idle power estimate (or any "do not account" power)
idle_power = 0

running_jobs['nodes'] = running_jobs['actual app'].map(job_size_overrides)
budgets = np.linspace(
    (running_jobs['nodes'] * 140).sum(),
    (running_jobs['nodes'] * 280).sum(),
    50)
running_jobs_sweep = list()
for budget in budgets:
    evaluate_jobs(budget, running_jobs, job_model_overrides)
    running_jobs_sweep.append(running_jobs.copy())

budget_df = pd.concat(running_jobs_sweep, keys=budgets, names=['budget']).reset_index()
if args.plot_path is not None:
    sns.set_theme(
        context='paper',
        style='whitegrid',
        rc={'font.size': 8.0}
    )
    plot_df = pd.melt(budget_df,
                     id_vars=['budget', 'actual app'],
                     value_vars=['even-slowdown slowdowns', 'even-cap slowdowns', 'guess-cap slowdowns'],
                     var_name='Budgeter',
                     value_name='slowdown')
    plot_df['Budgeter'] = plot_df['Budgeter'].map({
        'even-slowdown slowdowns': 'Even Slowdown (Ideal)',
        'even-cap slowdowns': 'Even Power Caps',
        'guess-cap slowdowns': 'Even Slowdown (Mispredict)'})
    plot_df['Job Type'] = plot_df['actual app']
    for actual_job, modeled_job in job_model_overrides.items():
        plot_df.loc[plot_df['actual app'] == actual_job, 'Job Type'] += f' modeled as {modeled_job}'
    low_power_order = (plot_df.loc[(plot_df['budget'] == plot_df['budget'].min()) & (plot_df['Budgeter'] == 'Even Power Caps')]
                       .groupby('Job Type')['slowdown']
                       .mean().sort_values(ascending=False).index.to_list())

    max_slowdown_df = plot_df.groupby(['budget', 'Budgeter'])['slowdown'].max().reset_index()
    balancer_order = ['Even Slowdown (Ideal)', 'Even Power Caps']
    balancer_dashes=[(1,0), (5, 2)]
    if len(args.confuse_jobs) > 0:
        balancer_order.append('Even Slowdown (Mispredict)')
        balancer_dashes.append((1, 2))

    if args.show_all_jobs:
        g = sns.relplot(
            height=3.0,
            data=plot_df,
            kind='line',
            x='budget',
            y='slowdown',
            hue='Job Type',
            hue_order=low_power_order,
            style='Budgeter',
            style_order=balancer_order,
            dashes=balancer_dashes,
        )
    else:
        g = sns.relplot(
            height=3.0,
            data=max_slowdown_df,
            kind='line',
            x='budget',
            y='slowdown',
            hue='Budgeter',
            hue_order=balancer_order,
            style='Budgeter',
            style_order=balancer_order,
            facet_kws=dict(legend_out=False),
        )
    #sns.move_legend(g, 'upper right')

    if args.show_title:
        g.fig.subplots_adjust(top=0.9)
        title=f'Max slowdown with {plot_df["Job Type"].nunique()} job types'
        if len(args.confuse_jobs) > 0:
            title += '\nMispredict {", ".join(args.confuse_jobs)}'
        g.fig.suptitle(title)

    g.set_axis_labels('Cluster Budget', 'Job Slowdown')
    g.ax.yaxis.set_major_formatter(mtick.PercentFormatter(1))
    #g.ax.set_ylim(0, 0.8)
    #g.ax.set_xlim(1700, 3600)
    g.savefig(args.plot_path, bbox_inches='tight', pad_inches=0)
    if args.print_csv:
        print(plot_df.to_csv(sys.stdout))

#if args.plot_path is not None:
#    budgets = np.linspace((140*nodes_per_job).sum()+1, (280*nodes_per_job).sum(), 200)
#    slowdowns = list()
#    all_caps = list()
#    for budget in budgets:
#        slowdown, selected_caps = get_slowdown_and_caps_at_budget(budget-idle_power, A, B, C, nodes_per_job)
#        slowdowns.append(slowdown)
#        all_caps.append(selected_caps)
#    caps_df = pd.DataFrame(all_caps, columns=running_jobs['actual app'], index=budgets)
#
#    sns.set(context='paper')
#
#    fig, (cap_ax, slowdown_ax) = plt.subplots(2, sharex=True, gridspec_kw={'height_ratios': [4, 1]})
#    caps_df.plot(ax=cap_ax)
#    cap_ax.set_ylabel('Power Cap per Node (W)')
#    cap_ax.set_xlabel('Cluster Budget (W)')
#    slowdown_ax.plot(budgets, slowdowns)
#    slowdown_ax.set_ylabel('Time Multiplier')
#    slowdown_ax.set_xlabel('Cluster Budget (W)')
#    fig.savefig(args.plot_path)
