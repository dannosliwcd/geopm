#!/usr/bin/env python3
import asyncio
import os
import sys
import time
import math
from sklearn import linear_model
from sklearn.preprocessing import PolynomialFeatures
import numpy as np
from typing import Dict
from datetime import datetime

DO_CROSS_JOB_SHARING = True

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

class EndpointMetrics:
    def __init__(self, host_count, initial_power_limit):
        self.measured_power = float('nan')
        self.current_power_cap = initial_power_limit
        self.host_count = host_count
        self.power_caps = list() # TODO: should probably be a size-bounded collection
        self.epoch_times = list() # TODO: should probably be a size-bounded collection
        self.last_recorded_epoch = float('nan')
        self.last_recorded_epoch_time = time.time()
        self.samples_in_last_model = 0
        self.max_achieved = float('nan')
        self.model = None

    def get_total_power_cap(self):
        return self.host_count * self.current_power_cap

    def get_total_measured_power(self):
        return self.host_count * self.measured_power

    def predict_time_at_power_cap(self, power_cap):
        return self.model.estimator_.intercept_ + self.model.estimator_.coef_[0] * power_cap + self.model.estimator_.coef_[1] * power_cap**2

    def predict_power_at_time(self, time):
        A = self.model.estimator_.coef_[1]
        B = self.model.estimator_.coef_[0]
        C = self.model.estimator_.intercept_
        if A == 0 and B == 0:
            # Time appears to be insensitive to power so far
            return POWER_MIN
        else:
            # Quadratic
            return (-B - np.sqrt(B**2 - 4 * A * (C - time))) / (2 * A)

    def predict_power_at_slowdown(self, slowdown):
        if self.model is None or slowdown < 1:
            return POWER_MAX
        else:
            time_power_max = self.predict_time_at_power_cap(POWER_MAX)
            if not time_power_max > 0:
                time_power_max = min(self.epoch_times)
            predicted_power = self.predict_power_at_time(slowdown * time_power_max)
            if math.isnan(predicted_power):
                A = self.model.estimator_.coef_[1]
                B = self.model.estimator_.coef_[0]
                C = self.model.estimator_.intercept_
                print(f'Warning: predicted NaN power(time_power_max={time_power_max}, A={A}, B={B}, C={C})', file=sys.stderr)
                return POWER_MAX
            # TODO: still more special cases, like "near zero" etc, plus cases
            # where only some runtimes have models so far. Maybe better to use
            # a constrained optimizer over predict_time().
            if predicted_power < POWER_MIN:
                return POWER_MIN
            return predicted_power


endpoints: Dict[str, EndpointMetrics] = dict()

barrier_entries = 0
pre_balance_barrier = asyncio.Semaphore(0)
barrier_entry_lock = asyncio.Lock()

time_of_last_power_target = time.time()
last_average_host_power_target = POWER_MAX

# Test P&R that cover the entire design-power range
P = 0.5 * (POWER_MIN + POWER_MAX)
R = 0.5 * (POWER_MAX - POWER_MIN)
power_delta_sign = -1

power_targets = None
try:
    with open('power_targets.csv') as f:
        power_targets = [float(line.strip()) for line in f if line]
except Exception as e:
    print('Unable to load power trace:', e)
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
    if now - time_of_last_power_target > 1.0:
        time_of_last_power_target = now
        if power_targets is None:
            if last_average_host_power_target >= POWER_MAX:
                power_delta_sign = -1
            if last_average_host_power_target < 180:
                power_delta_sign = 1
            last_average_host_power_target += 10 * power_delta_sign
        else:
            current_power_trace_index = (int((datetime.now() - start_time).total_seconds())
                                         // APPLY_TRACE_CAP_EVERY_N_SECONDS) * APPLY_TRACE_CAP_EVERY_N_SECONDS
            last_average_host_power_target = power_targets[current_power_trace_index] * R + P
    return last_average_host_power_target * EXPERIMENT_TOTAL_NODES


def calculate_balance_targets():
    # Warning: This is probably going to get a little confusing while I'm
    # figuring out which domains to use for things. Watch out for things like
    # endpoint_power_XYZ being measured in average watts per host instead of
    # sum-total across hosts in an endpoint.
    # TODO: Maybe make the balance_client transform to total-power-per-job
    #       so that the balance_server doesn't need to care about that?
    cluster_cap = get_cluster_power_target()

    if not DO_CROSS_JOB_SHARING:
        for endpoint in endpoints.values():
            endpoint.current_power_cap = cluster_cap / EXPERIMENT_TOTAL_NODES
        return

    active_hosts = sum(endpoint.host_count for endpoint in endpoints.values())
    idle_hosts = EXPERIMENT_TOTAL_NODES - active_hosts
    idle_power = idle_hosts * IDLE_WATTS_PER_NODE
    unallocated_power = cluster_cap - idle_power

    for endpoint in endpoints.values():
        # Everyone at least gets some power
        unallocated_power -= POWER_MIN * endpoint.host_count
        endpoint.current_power_cap = POWER_MIN

    for i in range(10):
        additional_power_needed_for_perf = {
            endpoint_addr: endpoint.host_count * (endpoint.predict_power_at_slowdown(1.05)
                                                  - endpoint.current_power_cap)
            for endpoint_addr, endpoint in endpoints.items()}
        total_power_needed_for_perf = sum(additional_power_needed_for_perf.values())

        additional_achievable_power = {
            endpoint_addr: endpoint.host_count * (POWER_MAX - endpoint.current_power_cap)
            for endpoint_addr, endpoint in endpoints.items()}
        total_achievable_power = sum(additional_achievable_power.values())

        budget_this_iteration = unallocated_power
        if (budget_this_iteration < 0.01 * cluster_cap) or not (total_achievable_power > 0):
            # We have allocated most of our budget or have no way to utilize our
            # budget. Stop trying to re-allocate.
            break
        for endpoint_addr, endpoint in endpoints.items():
            if total_power_needed_for_perf > 5*active_hosts:
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


async def rebalance_jobs(reader, writer):
    global pre_balance_barrier
    global barrier_entries
    global start_time
    endpoint_addr = writer.get_extra_info('peername')[0]

    if start_time is None:
        start_time = datetime.now()

    host_count_bytes = await reader.readline()
    host_count = int(host_count_bytes.decode())

    endpoint_power_bytes = await reader.readline()
    power_sample = float(endpoint_power_bytes.decode())
    initial_power_limit = float(POWER_MAX) # TODO: Could give NaN, then read-back what the applied limit was.
    print(f'{endpoint_addr}: {host_count} hosts, {power_sample} W')
    writer.write(f'{initial_power_limit}\n'.encode())
    await writer.drain()

    # Reset the barriers to wait on all connected endpoints
    async with barrier_entry_lock:
#TODOzz
        endpoints[endpoint_addr] = EndpointMetrics(host_count=host_count,
                                                   initial_power_limit=initial_power_limit)

    # There's an instance of this loop per job
    while True:
        received_endpoint_message = await reader.readline()
        if not received_endpoint_message:
            break
        received_endpoint_message = received_endpoint_message.decode()
        power_str, epoch_str = received_endpoint_message.split(',', 1)
        power_sample = float(power_str)
        epoch_sample = float(epoch_str)
        endpoint = endpoints[endpoint_addr]
        endpoint.measured_power = power_sample
        if not math.isnan(epoch_sample):
            if len(endpoint.epoch_times) == 0 or epoch_sample > endpoint.last_recorded_epoch:
                # TODO: This epoch&time stuff probably belongs on the balance client. Then just send coefficient updates here.
                new_epoch_time = time.time() # TODO: apply age of sample, include in message? Or maybe not important at this time scale?
                if epoch_sample > endpoint.last_recorded_epoch:
                    time_per_epoch  = ((new_epoch_time - endpoint.last_recorded_epoch_time)
                                       / (epoch_sample - endpoint.last_recorded_epoch))
                    if not np.isnan(time_per_epoch):
                        endpoint.epoch_times.append(time_per_epoch)
                        endpoint.power_caps.append(endpoint.current_power_cap) # TODO: running mean of power caps applied in that window
                endpoint.last_recorded_epoch = epoch_sample
                endpoint.last_recorded_epoch_time = new_epoch_time

                # Wait some time before training a new model
                if endpoint.samples_in_last_model + 5 < len(endpoint.epoch_times):
                    # Generate [x, x^2] values from X (power caps)
                    # Not including a bias column since the RANSACRegressor internally creates
                    # Linear regressors that have a built-in bias column
                    poly = PolynomialFeatures(2, include_bias=False)

                    def is_model_valid(model, *x_and_y):
                        # Check that time estimates are monotonically decreasing as we increase power
                        times = model.predict(poly.fit_transform(np.linspace(0, POWER_MAX, 8).reshape(-1, 1)))
                        return np.all(times[:-1] >= times[1:])
                    try:
                        endpoint.model = linear_model.RANSACRegressor(is_model_valid=is_model_valid)
                        sample_weight = np.ones(len(endpoint.power_caps))
                        # Give more importance to recent measurements
                        sample_weight[-10:] *= 5
                        endpoint.model.fit(poly.fit_transform(
                            np.reshape(endpoint.power_caps, (-1, 1))), endpoint.epoch_times, sample_weight=sample_weight)
                    except ValueError:
                        # The regressor detected that it found no valid models.
                        endpoint.model = None
                    if endpoint.model is not None and not is_model_valid(endpoint.model):
                        # The regressor found no valid models but didn't detect for some reason.
                        endpoint.model = None

                    if endpoint.model is not None:
                        endpoint.samples_in_last_model = len(endpoint.epoch_times)
        print(f"Received {received_endpoint_message} from {endpoint_addr}")

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
                cap = sum(endpoint.get_total_power_cap()
                          for endpoint in endpoints.values())
                measured = sum(endpoint.get_total_measured_power()
                               for endpoint in endpoints.values())
                cluster_server_trace.write(f'{timestamp},{target},{cap},{measured}\n')

        await pre_balance_barrier.acquire()
        pre_balance_barrier.release()
        async with barrier_entry_lock:
            barrier_entries -= 1
            if barrier_entries == 0:
                await pre_balance_barrier.acquire()

        new_power_target = endpoints[endpoint_addr].current_power_cap
        print(f"New target for {endpoint_addr}: {new_power_target}")
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
