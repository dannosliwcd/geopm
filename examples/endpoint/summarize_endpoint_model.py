#!/usr/bin/env python3
import argparse
import pandas as pd
import numpy as np
import sklearn
import matplotlib.pyplot as plt
from sklearn import linear_model
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import PolynomialFeatures, FunctionTransformer
from sklearn.model_selection import learning_curve
parser = argparse.ArgumentParser('Generate a summary of an endpoint experiment')
parser.add_argument('--cluster-trace', nargs='+')
parser.add_argument('--plot-path', default='model.png')
args = parser.parse_args()

df_list = list()
for trace in args.cluster_trace:
    df = pd.read_csv(trace, comment='#', sep=',')
    df['seconds per epoch'] = df['epoch duration'] / df['epoch'].diff()
    df['seconds per progress'] = df['progress duration'] / df['progress'].diff()
    df_list.append(df)
df = pd.concat(df_list)

epoch_columns = ['epoch duration', 'epoch', 'cap', 'seconds per epoch']
epoch_df = df[['time'] + epoch_columns].dropna()
# drop the epoch 0-1 transition (overly long)
epoch_df = epoch_df.loc[(epoch_df['epoch'] > 1) & (epoch_df['epoch duration'] > 0)]# & (epoch_df['epoch duration'] < 5)]
epoch_changes = (epoch_df.loc[epoch_df['epoch'].shift() != epoch_df['epoch']])[epoch_columns].dropna()
seconds_per_epoch = epoch_changes['seconds per epoch']
epoch_changes = epoch_changes.loc[seconds_per_epoch.notna()]
seconds_per_epoch = seconds_per_epoch.dropna()

progress_columns = ['progress duration', 'progress', 'progress cap', 'seconds per progress']
progress_df = df[['time'] + progress_columns].dropna()
# Drop the zero-duration samples (no measurements yet)
progress_df = progress_df.loc[progress_df['progress duration'] != 0]
progress_changes = (progress_df.loc[progress_df['progress'].shift() != progress_df['progress']])[progress_columns].dropna()
seconds_per_progress = progress_changes['seconds per progress']
progress_changes = progress_changes.loc[seconds_per_progress.notna()]
seconds_per_progress = seconds_per_progress.dropna()

def make_model():
    #TODO: reformulate for non-negative least squares
    # Better: AB^(x-140)+C
    # logb(y-C) = logb(AB^(x-140))
    #           = logb(A)(x-140)
    #...

    # y = A(TDP-x)^2 + B(TDP-x) + C, {A,B,C}>=0
    # --> i.e., min time is always at or above TDP (or some max limit)
    pipeline = Pipeline([
        ("transform_center", FunctionTransformer(lambda x: (280-x)**2)),
        #("polynomial_features", PolynomialFeatures(2, include_bias=False)),
        ("linear_regression", linear_model.LinearRegression(positive=True)),
    ])
    return pipeline

have_epochs = len(epoch_changes) > 0
print('Use epochs?', have_epochs)

if have_epochs:
    X = np.reshape(epoch_changes['cap'].values, (-1, 1))
    y = seconds_per_epoch
else:
    X = np.reshape(progress_changes['progress cap'].values, (-1, 1))
    y = seconds_per_progress

model = make_model()
model.fit(X, y)
y_train_pred = model.predict(X)

r2score = sklearn.metrics.r2_score(y, y_train_pred)
print('Training R2 Score:', r2score)
print(f'A: {model.named_steps["linear_regression"].coef_}, C: {model.named_steps["linear_regression"].intercept_}')

def plot_cap_and_epoch(df, ax, use_progress=False):
    if use_progress:
        (280-df.set_index('time')['progress cap']).plot(ax=ax)
    else:
        (280-df.set_index('time')['cap']).plot(ax=ax)
    ax.set_ylabel('TDP - cap')
    ax.yaxis.label.set_color(ax.get_lines()[0].get_color())
    ax.spines["left"].set_edgecolor(ax.get_lines()[0].get_color())
    ax.tick_params(axis='y', colors=ax.get_lines()[0].get_color())

    ax_epochpower = ax.twinx()
    if use_progress:
        plot_df = df.set_index('time')
        # TODO: why do I need to diff progress but not epoch? Is one of them sending diff from agent and other sending raw?
        (plot_df['progress duration'] / plot_df['progress'].diff()).plot(ax=ax_epochpower, c='r')
        ax_epochpower.set_ylabel('Progress=1 Duration (s)')
    else:
        plot_df = df.set_index('time')
        (plot_df['seconds per epoch']).plot(ax=ax_epochpower, c='r')
        ax_epochpower.set_ylabel('Epoch Duration (s)')
    ax_epochpower.yaxis.label.set_color(ax_epochpower.get_lines()[0].get_color())
    ax_epochpower.spines["right"].set_edgecolor(ax_epochpower.get_lines()[0].get_color())
    ax_epochpower.tick_params(axis='y', colors=ax_epochpower.get_lines()[0].get_color())


fig, ((ax_timepower,  ax_learningcurve),
      (ax_epochpower, ax_progresspower)) = plt.subplots(2, 2, figsize=(9.35, 5.8))
if True or have_epochs:
    ax_timepower.scatter(X, y, label='True')
    plot_order = X.flatten().argsort()
    ax_timepower.plot(X[plot_order], y_train_pred[plot_order], label='Predicted', c='k')
ax_timepower.set_xlabel('cap')
ax_timepower.set_ylabel('time per epoch')


fig.subplots_adjust(wspace=0.5, hspace=0.3)
plot_cap_and_epoch(epoch_df, ax_epochpower)
plot_cap_and_epoch(progress_df, ax_progresspower, use_progress=True)
ax_epochpower.set_xlim(df['time'].min())
ax_progresspower.set_xlim(df['time'].min())

if True or have_epochs:
    train_sizes, train_scores, test_scores = learning_curve(
        make_model(), X, y, scoring='neg_mean_absolute_percentage_error', train_sizes=np.arange(2 if len(y)<50 else 5, len(y)//2, 1), cv=5)
    train_scores_mean = np.mean(train_scores, axis=1)
    train_scores_std = np.std(train_scores, axis=1)
    test_scores_mean = np.mean(test_scores, axis=1)
    test_scores_std = np.std(test_scores, axis=1)
    ax_learningcurve.fill_between(
        train_sizes,
        train_scores_mean - train_scores_std,
        train_scores_mean + train_scores_std,
        alpha=0.1,
        color="r",
    )
    ax_learningcurve.fill_between(
        train_sizes,
        test_scores_mean - test_scores_std,
        test_scores_mean + test_scores_std,
        alpha=0.1,
        color="g",
    )
    ax_learningcurve.plot(train_sizes, train_scores_mean, "o-", color="r", label="Training score")
    ax_learningcurve.plot(train_sizes, test_scores_mean, "o-", color="g", label="Cross-validation score")
    ax_learningcurve.legend(loc="best")
    ax_learningcurve.set_xlabel('Training Size')
    ax_learningcurve.set_ylabel('-MAPE')

fig.savefig(args.plot_path, bbox_inches='tight')
