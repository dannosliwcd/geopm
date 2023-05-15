#!/usr/bin/env python3
import asyncio
import os
import sys
import time
import math
import pandas as pd
import subprocess
from sklearn import linear_model
from sklearn.preprocessing import PolynomialFeatures, FunctionTransformer
from scipy import optimize
import numpy as np
from typing import Dict
from datetime import datetime, timedelta
import argparse

from yaml import load
try:
    from yaml import CSafeLoader as SafeLoader
except ImportError:
    from yaml import SafeLoader

parser = argparse.ArgumentParser(
    description='Launch the cluster-management side of the GEOPM endpoint.')
parser.add_argument('--no-cross-job-sharing', action='store_true')
parser.add_argument('--replay-job-trace',
                    help='Path to a csv of job submission times to replay. '
                         'Must have jobTypeID(int) and startTime(int) columns. '
                         'jobTypeID is the index to a job type in '
                         '--job-names. startTime is number of seconds since '
                         'this script started running.')
parser.add_argument('--job-names', nargs='*',
                    help='Names of jobs as defined in --app-info, ordered by jobTypeID '
                         'as used in --replay-job-trace')
parser.add_argument('--job-weights', default='', help='Weights to apply to queues of job types in the scheduler')
parser.add_argument('--average-power-target', type=float,
                    help='Average power target to send to this cluster')
parser.add_argument('--replay-start-time', action='store_true',
                    help='Replay job start times instead of calculating at run time.')
parser.add_argument('--reserve', type=float,
                    help='Maximum power reserve offered by this cluster, above '
                         'and below the average power target')
parser.add_argument('--use-pre-characterized', action='store_true')
parser.add_argument('--ignore-run-time-models', action='store_true')
parser.add_argument('--confuse-jobs', nargs='*', default=[],
                    help='Confuse one job for another. E.g., to use the ep.D.x model '
                         'for bt.D.x, use --confuse-jobs bt.D.x=ep.D.x')
parser.add_argument('--app-info',
                    default='app_properties.yaml',
                    help='Path to application characterization data')
args = parser.parse_args()

if args.replay_job_trace is not None and args.job_names is None:
    parser.error('Must specify --job-names if --replay-job-trace is used.')

with open(args.app_info) as f:
    app_info = load(f, Loader=SafeLoader)

JOB_PATHS = [] if args.job_names is None else [app_info['applications'][name]['launcher']
                                               for name in args.job_names]

JOB_SIZES_BY_NAME = {
    name: data['nodes']
    for name, data in app_info['applications'].items()
}

from collections import namedtuple
Coef = namedtuple('coef', ['A','C'])
USE_PRE_CHARACTERIZED = args.use_pre_characterized
IGNORE_RUN_TIME_MODELS = args.ignore_run_time_models
MODEL_COEFFICIENTS_BY_PROFILE_TAIL = {
    name: Coef(data['model']['A'], data['model']['C'])
    for name, data in app_info['applications'].items()
}

# TODO for testing small case
# MODEL_COEFFICIENTS_BY_PROFILE_TAIL['bt.C.x'] = Coef(5.47820496e-05, 1.4833673118803585)
# MODEL_COEFFICIENTS_BY_PROFILE_TAIL['sp.C.x'] = Coef(1.96963115e-05, 2.153331623883713)

for confusion in args.confuse_jobs:
    actual_job, modeled_job = confusion.split('=', 1)
    MODEL_COEFFICIENTS_BY_PROFILE_TAIL[actual_job] = MODEL_COEFFICIENTS_BY_PROFILE_TAIL[modeled_job]

JOB_WEIGHTS = np.clip([float(w) for w in args.job_weights.split(',') if len(w)>0], 0, None)
JOB_NAMES = args.job_names if args.job_names is not None else []
JOB_SIZES = np.array([JOB_SIZES_BY_NAME[name] for name in JOB_NAMES])
pending_new_hosts = 0

GEOPM_ENDPOINT_SOCKET = 63094  # should be configurable
GEOPM_ENDPOINT_SERVER_HOST = os.getenv('GEOPM_ENDPOINT_SERVER_HOST')
POWER_MAX=280
POWER_MIN=140
if GEOPM_ENDPOINT_SERVER_HOST is None:
    print('Error: Must set env var GEOPM_ENDPOINT_SERVER_HOST', file=sys.stderr)
    sys.exit(1)


def get_total_node_count():
    EXPERIMENT_TOTAL_NODES = os.getenv('EXPERIMENT_TOTAL_NODES')
    if EXPERIMENT_TOTAL_NODES is None:
        print('Error: Must set env var EXPERIMENT_TOTAL_NODES', file=sys.stderr)
        sys.exit(1)
    return int(EXPERIMENT_TOTAL_NODES)


EXPERIMENT_TOTAL_NODES = get_total_node_count()
IDLE_WATTS_PER_NODE = 38
print(f'Testing with {EXPERIMENT_TOTAL_NODES} nodes')

class EndpointMetrics:
    def __init__(self, host_count, initial_power_limit, profile):
        self.measured_power = float('nan')
        self.current_power_cap = initial_power_limit
        self.host_count = host_count
        self.profile = profile
        self.power_caps = list() # TODO: should probably be a size-bounded collection
        self.epoch_times = list() # TODO: should probably be a size-bounded collection
        self.progress_caps = list() # TODO: should probably be a size-bounded collection
        self.progress_times = list() # TODO: should probably be a size-bounded collection
        self.last_recorded_epoch = float(1) # skip the first epoch (which includes pre-epoch time)
        self.last_recorded_epoch_time = time.time()
        self.last_recorded_progress = float('nan')
        self.last_recorded_progress_time = time.time()
        self.samples_in_last_model = 0
        self.max_achieved = float('nan')
        self.model = None

    def get_total_power_cap(self):
        return self.host_count * self.current_power_cap

    def get_total_measured_power(self):
        return self.host_count * self.measured_power

    def predict_time_at_power_cap(self, power_cap):
        #return np.exp(self.model.estimator_.intercept_ + self.model.estimator_.coef_[0] * power_cap + self.model.estimator_.coef_[1] * power_cap**2)

        #return np.exp(self.model.intercept_ + self.model.coef_[0] * power_cap + self.model.coef_[1] * power_cap**2)
        #poly = PolynomialFeatures(2, include_bias=False)
        if USE_PRE_CHARACTERIZED and (self.model is None or IGNORE_RUN_TIME_MODELS):
            # A, C = MODEL_COEFFICIENTS_BY_PROFILE_TAIL[self.profile.rsplit('/', 1)[-1].rsplit('"',1)[0]]
            A, C = MODEL_COEFFICIENTS_BY_PROFILE_TAIL[self.profile.rsplit('=', 1)[-1].strip('"\n')]
            return A * (POWER_MAX - power_cap)**2 + C
        poly = FunctionTransformer(lambda x: (POWER_MAX-x)**2)
        times = self.model.predict(poly.fit_transform(np.reshape([[power_cap]], (-1, 1))))
        return times[0]
        #return (self.model.intercept_ + self.model.coef_[0] * power_cap + self.model.coef_[1] * power_cap**2)

    def predict_power_at_time(self, time):
        #A = self.model.estimator_.coef_[1]
        #B = self.model.estimator_.coef_[0]
        #C = self.model.estimator_.intercept_
        if USE_PRE_CHARACTERIZED and (self.model is None or IGNORE_RUN_TIME_MODELS):
            # A, C = MODEL_COEFFICIENTS_BY_PROFILE_TAIL[self.profile.rsplit('/', 1)[-1].rsplit('"',1)[0]]
            A, C = MODEL_COEFFICIENTS_BY_PROFILE_TAIL[self.profile.rsplit('=', 1)[-1].strip('"\n')]
            B = 0
        else:
            A = self.model.coef_[0]
            #B = self.model.coef_[0]
            B = 0
            C = self.model.intercept_

        #if abs(A) <= 1e-5 and abs(B) < 0.001:
        if A == 0:
            return POWER_MIN
        else:
            # Quadratic
            #return (-B - np.sqrt(B**2 - 4 * A * (C - time))) / (2 * A)
            return POWER_MAX-(-B + np.sqrt(B**2 - 4 * A * (C - time))) / (2 * A)

    def predict_power_at_slowdown(self, slowdown):
        if slowdown < 1:
            return POWER_MAX
        if self.model is None and not USE_PRE_CHARACTERIZED:
            print('no model')
            return max(POWER_MIN, min(POWER_MAX, (POWER_MAX/slowdown + POWER_MIN) / 2)) # or take mean of the rest of the job types
        else:
            time_power_max = self.predict_time_at_power_cap(POWER_MAX)
            if not time_power_max > 0:
                time_power_max = min(self.epoch_times)
            predicted_power = self.predict_power_at_time(slowdown * time_power_max)
            if math.isnan(predicted_power):
                # A = self.model.coef_[1]
                # B = self.model.coef_[0]
                # C = self.model.intercept_
                # Fall back to linear
                power = max(POWER_MIN, min(POWER_MAX, POWER_MAX / slowdown / 2))
                # print(f'model predicted nan power, A={A}, B={B}, C={C}')
                # print(f'Warning: Fall back to {power} (time_power_max={time_power_max}, A={A}, B={B}, C={C})', file=sys.stderr)
                return power
            # TODO: still more special cases, like "near zero" etc, plus cases
            # where only some runtimes have models so far. Maybe better to use
            # a constrained optimizer over predict_time().
            if predicted_power < POWER_MIN:
                return POWER_MIN
            # A = self.model.coef_[0]
            # B = self.model.coef_[0]
            # B = 0
            # C = self.model.intercept_
            # print(f'Predicted power(slowdown={slowdown}, time_power_max={time_power_max}) = {predicted_power}, A={A}, B={B}, C={C} ({self.profile})', file=sys.stderr)
            return predicted_power


endpoints: Dict[str, EndpointMetrics] = dict()

barrier_entries = 0
pre_balance_barrier = asyncio.Semaphore(0)
barrier_entry_lock = asyncio.Lock()

time_of_last_power_target = time.time()

# Test P&R that cover the entire design-power range by default. Else, use
# what the args say to use.
P = 0.5 * (POWER_MIN + POWER_MAX) * EXPERIMENT_TOTAL_NODES
if args.average_power_target is not None:
    P = args.average_power_target
R = 0.5 * (POWER_MAX - POWER_MIN) * EXPERIMENT_TOTAL_NODES
if args.reserve is not None:
    R = args.reserve
print(f'Testing with P={P} and R={R}')
power_delta_sign = -1

last_average_host_power_target = P / EXPERIMENT_TOTAL_NODES

power_targets = None
print('WARNING: disabled power traces... should probably just make it an argparse option')
time.sleep(0.25)
try:
    with open('power_targets.csv') as f:
        power_targets = [float(line.strip()) for line in f if line]
except Exception as e:
    print('Unable to load power trace:', e)
    pass

job_queue_trace = None
if args.replay_job_trace is not None:
    try:
        job_queue_trace = pd.read_csv(args.replay_job_trace).sort_values('startTime')
        job_queue_trace['scheduled'] = False
    except Exception as e:
        print('Unable to load job trace:', e)
        pass

start_time = None
current_power_trace_index = 0
APPLY_TRACE_CAP_EVERY_N_SECONDS = 4
cluster_server_trace = open('cluster_server_trace.csv', 'w')
cluster_server_trace.write('timestamp,target,cap,measured\n')

# TODO: needs to be time-varying
def get_cluster_power_target():
    global last_average_host_power_target
    global power_delta_sign
    global time_of_last_power_target
    global current_power_trace_index
    now = time.time()
    mean_p_per_node = P / EXPERIMENT_TOTAL_NODES
    r_per_node = R / EXPERIMENT_TOTAL_NODES
    if now - time_of_last_power_target > 1.0:
        time_of_last_power_target = now
        if power_targets is None:
            power_step = 10 if r_per_node > 0 else 0
            if last_average_host_power_target >= mean_p_per_node + r_per_node:
                power_delta_sign = -1
            if last_average_host_power_target <= mean_p_per_node - r_per_node:
                power_delta_sign = 1
            last_average_host_power_target += power_step * power_delta_sign
        else:
            current_power_trace_index = (int((datetime.now() - start_time).total_seconds())
                                         // APPLY_TRACE_CAP_EVERY_N_SECONDS) * APPLY_TRACE_CAP_EVERY_N_SECONDS
            last_average_host_power_target = (power_targets[current_power_trace_index] * R + P) / EXPERIMENT_TOTAL_NODES
    return last_average_host_power_target * EXPERIMENT_TOTAL_NODES


def get_ready_jobs(ready_hosts):
    time_now = int((datetime.now() - start_time).total_seconds())
    global pending_new_hosts

    launch_list = []

    if job_queue_trace is not None:
        if args.replay_start_time:
            # Use startTime for AQA and queueTime for non-aqa schedule
            is_ready = ((job_queue_trace['startTime'] <= time_now) & (job_queue_trace['startTime'] >= 0) & (~job_queue_trace['scheduled']))

            # Queued jobs, already sorted by AQA start time
            for idx, job in job_queue_trace.loc[is_ready].iterrows():
                job_path = JOB_PATHS[job['jobTypeID']]

                # launch_list.append(job_path)
                # job_queue_trace.loc[idx, 'scheduled'] = True

                job_size = JOB_SIZES[job['jobTypeID']]
                if job_size <= ready_hosts - pending_new_hosts:
                    launch_list.append(job_path)
                    ready_hosts -= job_size
                    pending_new_hosts += job_size
                    job_queue_trace.loc[idx, 'scheduled'] = True
                else:
                    break
        else:
            # One queue per path in JOB_PATHS
            is_ready = ((job_queue_trace['queueTime'] <= time_now) & (job_queue_trace['startTime'] >= 0) & (~job_queue_trace['scheduled']))
            ready_jobs = job_queue_trace.loc[is_ready]
            ready_jobs_by_type = ready_jobs.groupby('jobTypeID').size()
            waiting_job_types = ready_jobs_by_type.index[ready_jobs_by_type > 0]
            if len(waiting_job_types) == 0:
                return []
            non_waiting_job_types = ready_jobs_by_type.index[ready_jobs_by_type == 0]

            # Ensure that sum(weights) == 1 for weights of jobs that are waiting to run
            total_usable_weight = JOB_WEIGHTS[waiting_job_types.values].sum()
            scheduler_weights = JOB_WEIGHTS / total_usable_weight
            scheduler_weights[non_waiting_job_types] = 0

            # Allocate hosts to job-type queues by their weights. We only do
            # exclusive host allocations, so round to a whole number.
            host_counts_by_queue = (ready_hosts * scheduler_weights).round()

            # If any ready hosts aren't allocated to a queue, shuffle them around
            # If too many are allocated in total, randomly steal the deficit
            surplus_hosts = ready_hosts - host_counts_by_queue.sum()
            try:
                surplus_host_assignees = np.random.randint(len(host_counts_by_queue), size=int(abs(surplus_hosts)))
            except:
                import pdb; pdb.set_trace()
            host_counts_by_queue[surplus_host_assignees] += surplus_hosts // len(surplus_host_assignees)

            job_launches_by_queue = host_counts_by_queue // JOB_SIZES
            for job_type, job_launch_count in enumerate(job_launches_by_queue):
                launch_candidates = ready_jobs.loc[ready_jobs['jobTypeID'] == job_type]
                # Launchable count: as many of the ready job of this type as our job weights allow
                launchable_job_count = min(len(launch_candidates), int(job_launch_count))
                launched_jobs = launch_candidates.iloc[:launchable_job_count]['jobID']
                job_queue_trace.loc[job_queue_trace['jobID'].isin(launched_jobs), 'scheduled'] = True
                launch_list.extend([JOB_PATHS[job_type]] * launchable_job_count)
    return launch_list


def calculate_balance_targets():
    # Warning: This is probably going to get a little confusing while I'm
    # figuring out which domains to use for things. Watch out for things like
    # endpoint_power_XYZ being measured in average watts per host instead of
    # sum-total across hosts in an endpoint.
    # TODO: Maybe make the balance_client transform to total-power-per-job
    #       so that the balance_server doesn't need to care about that?
    cluster_cap = get_cluster_power_target()

    active_hosts = sum(endpoint.host_count for endpoint in endpoints.values())
    idle_hosts = EXPERIMENT_TOTAL_NODES - active_hosts
    idle_power = idle_hosts * IDLE_WATTS_PER_NODE
    unallocated_power = cluster_cap - idle_power

    for endpoint in endpoints.values():
        # Everyone at least gets some power
        unallocated_power -= POWER_MIN * endpoint.host_count
        endpoint.current_power_cap = POWER_MIN

    if args.no_cross_job_sharing:
        additional_power_per_host = unallocated_power / active_hosts
        for endpoint in endpoints.values():
            endpoint.current_power_cap += additional_power_per_host
            endpoint.current_power_cap = max(POWER_MIN, min(POWER_MAX, endpoint.current_power_cap))
        return

    def power_deficit_at_slowdown(slowdown):
        """Return the power budget deficit if all jobs were given the power they
        want for a given slowdown factor.
        """
        job_power = sum(endpoint.host_count * endpoint.predict_power_at_slowdown(slowdown)
                        for endpoint in endpoints.values())
        #print(f'deficit at slow={slowdown} ({job_power}+{idle_power}-{cluster_cap}):', job_power+idle_power-cluster_cap, file=sys.stderr)
        return job_power + idle_power - cluster_cap

    slowdown_lower_bound = 1.0
    slowdown_upper_bound = 4
    try:
        # At what slowdown do all jobs have the same expected slowdown while in
        # aggregate they are expected to consume exactly the power budget.
        target_slowdown = optimize.bisect(power_deficit_at_slowdown, slowdown_lower_bound, slowdown_upper_bound)
        print('target balanced slowdown:', target_slowdown)
    except ValueError as e:
        ## May happen if too many idle servers, too few job types have models, or not enough performance sensitivity across all types
        target_slowdown = POWER_MAX / ((cluster_cap - idle_power) / active_hosts)
        print(f'fallback to target_slowdown={target_slowdown}. cap={cluster_cap}, idle={idle_power}', e, file=sys.stderr)

    for i in range(10):
        additional_power_needed_for_perf = {
            endpoint_addr: endpoint.host_count * (endpoint.predict_power_at_slowdown(target_slowdown)
                                                  - endpoint.current_power_cap)
            for endpoint_addr, endpoint in endpoints.items()}
        total_power_needed_for_perf = sum(additional_power_needed_for_perf.values())
        # for endpoint_addr, endpoint in endpoints.items():
        #     print(endpoint_addr, 'power at 1x:', endpoint.predict_power_at_slowdown(1), '... at 1.5x:', endpoint.predict_power_at_slowdown(1.5))



        additional_achievable_power = {
            endpoint_addr: endpoint.host_count * (POWER_MAX - endpoint.current_power_cap)
            for endpoint_addr, endpoint in endpoints.items()}
        total_achievable_power = sum(additional_achievable_power.values())

        budget_this_iteration = unallocated_power
        if (budget_this_iteration < 1) or not (total_achievable_power > 0):
            # We have allocated most of our budget or have no way to utilize our
            # budget. Stop trying to re-allocate.
            break
        for endpoint_addr, endpoint in endpoints.items():
            if total_power_needed_for_perf > 5*active_hosts and i < 5:
                # TODO: Needs to be > 0 to be a valid calculation. But needs to
                #       be >> 0 so the "achievable" phase ever kicks in. What
                #       is a realistic cutoff to use? For now, using 5W/host
                # We haven't met our performance needs yet. Keep budgeting based on those.
                power_to_alloc = (additional_power_needed_for_perf[endpoint_addr]
                                  / total_power_needed_for_perf
                                  * budget_this_iteration)
            elif total_achievable_power > 0:
                # Expected performance needs are met. Budget based on achievable power
                power_to_alloc = (additional_achievable_power[endpoint_addr]
                                  / total_achievable_power
                                  * budget_this_iteration)
            unallocated_power -= power_to_alloc
            endpoint.current_power_cap += power_to_alloc / endpoint.host_count
            # Limit per-host power:
            over_budgeted_host_power = max(0, endpoint.current_power_cap - POWER_MAX)
            unallocated_power += over_budgeted_host_power * endpoint.host_count
            endpoint.current_power_cap -= over_budgeted_host_power
    #print('unallocated:', unallocated_power, '  true unalloc:', cluster_cap - sum(endpoint.get_total_power_cap() for endpoint in endpoints.values()) - idle_power)


async def rebalance_jobs(reader, writer):
    global pre_balance_barrier
    global barrier_entries
    global pending_new_hosts
    endpoint_addr = writer.get_extra_info('peername')[0]

    print(f'New endpoint {endpoint_addr}', file=sys.stderr)
    host_count_bytes = await reader.readline()
    host_count = int(host_count_bytes.decode())
    pending_new_hosts -= host_count

    endpoint_power_bytes = await reader.readline()
    power_sample = float(endpoint_power_bytes.decode())
    initial_power_limit = float(POWER_MAX) # TODO: Could give NaN, then read-back what the applied limit was.

    endpoint_profile_bytes = await reader.readline()
    endpoint_profile = endpoint_profile_bytes.decode()
    print(f'{endpoint_addr}: {host_count} hosts, {power_sample} W')
    writer.write(f'{initial_power_limit}\n'.encode())
    await writer.drain()

    # Reset the barriers to wait on all connected endpoints
    async with barrier_entry_lock:
#TODOzz
        endpoints[endpoint_addr] = EndpointMetrics(host_count=host_count,
                                                   initial_power_limit=initial_power_limit,
                                                   profile=endpoint_profile)

    # There's an instance of this loop per job
    while True:
        received_endpoint_message = await reader.readline()
        if not received_endpoint_message:
            break
        received_endpoint_message = received_endpoint_message.decode()
        (power_str,
         epoch_str, epoch_cap_str, epoch_duration_str,
         progress_str, progress_cap_str, progress_duration_str) = received_endpoint_message.split(',', 6)
        power_sample = float(power_str)
        epoch_sample = float(epoch_str)
        epoch_cap = float(epoch_cap_str)
        epoch_duration = float(epoch_duration_str)
        progress_sample = float(progress_str)
        progress_cap = float(progress_cap_str)
        progress_duration = float(progress_duration_str)
        endpoint = endpoints[endpoint_addr]
        endpoint.measured_power = power_sample

        new_time = time.time()
        if progress_sample > endpoint.last_recorded_progress and progress_duration > 0 and not math.isnan(progress_cap):
            endpoint.progress_times.append(progress_duration)
            endpoint.progress_caps.append(progress_cap)
        endpoint.last_recorded_progress = progress_sample
        endpoint.last_recorded_progress_time = new_time

        if epoch_sample > endpoint.last_recorded_epoch and epoch_duration > 0 and not math.isnan(epoch_cap):
            endpoint.epoch_times.append(epoch_duration)
            endpoint.power_caps.append(epoch_cap)
        endpoint.last_recorded_epoch = epoch_sample
        endpoint.last_recorded_epoch_time = new_time

        # Use epoch data if it is available. Otherwise, use region progress
        # TODO: This works better if those metrics are exclusive. Should model
        # them together instead.
        if len(endpoint.epoch_times) > 20:
            times = endpoint.epoch_times
            caps = endpoint.power_caps
        else:
            times = endpoint.progress_times
            caps = endpoint.progress_caps

        # Wait some time before training a new model
        if endpoint.samples_in_last_model + 10 < len(times):
            # Generate [x, x^2] values from X (power caps)
            # Not including a bias column since the RANSACRegressor internally creates
            # Linear regressors that have a built-in bias column
            #poly = PolynomialFeatures(2, include_bias=False)
            poly = FunctionTransformer(lambda x: (POWER_MAX-x)**2)

            def is_model_valid(model, *x_and_y):
                # Check that time estimates are monotonically decreasing as we increase power
                #times = np.exp(model.predict(poly.fit_transform(np.linspace(0, POWER_MAX, 8).reshape(-1, 1))))
                times = model.predict(poly.fit_transform(np.linspace(0, POWER_MAX, 8).reshape(-1, 1)))
                return np.all(times[:-1] >= times[1:])
            try:
                #endpoint.model = linear_model.RANSACRegressor(is_model_valid=is_model_valid)
                class Model:
                    pass
                if endpoint.model is None:
                    print(f'New model for {endpoint.profile}')
                endpoint.model = linear_model.LinearRegression(positive=True)
                # sample_weight = np.ones(len(caps))
                # Give more importance to recent measurements
                # sample_weight[-10:] *= 5
                endpoint.model.fit(poly.fit_transform(
                    np.reshape(caps, (-1, 1))), times)#, sample_weight=sample_weight)
            except ValueError as e:
                print('Warning: model error.', e)
                print('Model inputs:', list(zip(caps, times)))
                # The regressor detected that it found no valid models.
                endpoint.model = None
            if endpoint.model is not None and not is_model_valid(endpoint.model):
                print('Warning: invalid model.')
                # The regressor found no valid models but didn't detect for some reason.
                endpoint.model = None

            if endpoint.model is not None:
                endpoint.samples_in_last_model = len(times)
        #print(f"Received {received_endpoint_message} from {endpoint_addr}")

        # Wait for all new samples to arrive before rebalancing the budget
        # Python 3.6 doesn't have a barrier implementation. Use a semaphore+lock instead
        async with barrier_entry_lock:
            barrier_entries += 1
            # print('entries',barrier_entries, 'out of', len(endpoint_host_counts))
            if barrier_entries == len(endpoints):
                # Make a single exit per barrier trigger a rebalancing calculation
                calculate_balance_targets()
                pre_balance_barrier.release()
                timestamp = datetime.now().strftime('%a %b %d %H:%M:%S.%f %Y')
                target = last_average_host_power_target * EXPERIMENT_TOTAL_NODES

                active_hosts = sum(endpoint.host_count for endpoint in endpoints.values())
                idle_hosts = EXPERIMENT_TOTAL_NODES - active_hosts
                idle_power = idle_hosts * IDLE_WATTS_PER_NODE

                cap = sum(endpoint.get_total_power_cap()
                          for endpoint in endpoints.values()) + idle_power
                measured = sum(endpoint.get_total_measured_power()
                               for endpoint in endpoints.values()) + idle_power
                cluster_server_trace.write(f'{timestamp},{target},{cap},{measured}\n')

                # Launch new jobs from our experiment queue
                active_hosts = sum(endpoint.host_count for endpoint in endpoints.values())
                need_active_hosts_for_cap = min(EXPERIMENT_TOTAL_NODES, math.ceil(target / POWER_MAX))
                additional_active_hosts = need_active_hosts_for_cap - active_hosts
                if additional_active_hosts > 0:
                    job_script_paths = get_ready_jobs(additional_active_hosts)
                else:
                    job_script_paths = []
                for path in job_script_paths:
                    print('Launch:', path)
                    # Note: The launched processes require that
                    # GEOPM_ENDPOINT_SERVER_HOST is set. This script also has
                    # that requirement, so it's passed through anyways.
                    subprocess.Popen(['sbatch', path])

        await pre_balance_barrier.acquire()
        pre_balance_barrier.release()
        async with barrier_entry_lock:
            barrier_entries -= 1
            if barrier_entries == 0:
                await pre_balance_barrier.acquire()

        new_power_target = endpoints[endpoint_addr].current_power_cap
        #print(f"New target for {endpoint_addr}: {new_power_target}")
        job_power_limit = f'{new_power_target}\n'

        writer.write(job_power_limit.encode())
        await writer.drain()
        await asyncio.sleep(0.25)

    async with barrier_entry_lock:
        del endpoints[endpoint_addr]
    print(f"Close the client {endpoint_addr} socket")
    writer.close()
    async with barrier_entry_lock:
        if barrier_entries == len(endpoints) and barrier_entries > 0:
            # Someone is waiting for this endpoint to enter the barrier. Let's tidy
            # that up for them.
            calculate_balance_targets()
            pre_balance_barrier.release()

loop = asyncio.get_event_loop()
server = loop.run_until_complete(asyncio.start_server(rebalance_jobs,
                                                      GEOPM_ENDPOINT_SERVER_HOST,
                                                      GEOPM_ENDPOINT_SOCKET,
                                                      loop=loop))

if job_queue_trace is None:
    start_time = datetime.now()
else:
    # Fast-forward to the first job launch.
    start_time = datetime.now() - timedelta(seconds=int(job_queue_trace['startTime'].min()))
    for idx, path in enumerate(get_ready_jobs(EXPERIMENT_TOTAL_NODES)):
        print('Launch:', path)
        subprocess.Popen(['sbatch', f'--begin=now+{idx*3}', path])

# Serve requests until Ctrl+C is pressed
print('Serving on {}'.format(server.sockets[0].getsockname()
                             if server.sockets is not None else '<unknown>'))
try:
    loop.run_forever()
except KeyboardInterrupt:
    pass

# Close the server
server.close()
loop.run_until_complete(server.wait_closed())
loop.close()
