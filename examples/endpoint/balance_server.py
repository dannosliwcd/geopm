#!/usr/bin/env python3
import asyncio
import os
import sys
import time

GEOPM_ENDPOINT_SOCKET = 63094  # should be configurable
GEOPM_ENDPOINT_SERVER_HOST = os.getenv('GEOPM_ENDPOINT_SERVER_HOST')
if GEOPM_ENDPOINT_SERVER_HOST is None:
    print('Error: Must set env var GEOPM_ENDPOINT_SERVER_HOST', file=sys.stderr)
    sys.exit(1)

EXPERIMENT_TOTAL_NODES = os.getenv('EXPERIMENT_TOTAL_NODES')
if EXPERIMENT_TOTAL_NODES is None:
    print('Error: Must set env var EXPERIMENT_TOTAL_NODES', file=sys.stderr)
    sys.exit(1)

EXPERIMENT_TOTAL_NODES = int(EXPERIMENT_TOTAL_NODES)
IDLE_WATTS_PER_NODE = 38

endpoint_power_measurements = dict()
endpoint_host_counts = dict()
endpoint_power_caps = dict()

barrier_entries = 0
pre_balance_barrier = asyncio.Semaphore(0)
barrier_entry_lock = asyncio.Lock()

time_of_last_power_target = time.time()
last_average_host_power_target = 280
P = 240
R = 40
power_delta_sign = -1

power_targets = None
try:
    with open('power_targets.csv') as f:
        power_targets = list(reversed([float(line.strip()) for line in f if line]))
except Exception as e:
    print('Unable to load power trace:', e)
    pass

# TODO: needs to be time-varying
def get_cluster_power_target():
    global last_average_host_power_target
    global power_delta_sign
    global time_of_last_power_target
    now = time.time()
    if now - time_of_last_power_target > 1.0:
        time_of_last_power_target = now
        if power_targets is None:
            if last_average_host_power_target >= 280:
                power_delta_sign = -1
            if last_average_host_power_target < 180:
                power_delta_sign = 1
            last_average_host_power_target += 10 * power_delta_sign
        else:
            last_average_host_power_target = power_targets.pop() * R + P
    return last_average_host_power_target * sum(endpoint_host_counts.values())


def calculate_balance_targets():
    # Warning: This is probably going to get a little confusing while I'm
    # figuring out which domains to use for things. Watch out for things like
    # endpoint_power_XYZ being measured in average watts per host instead of
    # sum-total across hosts in an endpoint.
    # TODO: Maybe make the balance_client transform to total-power-per-job
    #       so that the balance_server doesn't need to care about that?
    cluster_cap = get_cluster_power_target()
    idle_hosts = sum(endpoint_host_counts.values())
    allocated_power = sum(endpoint_power_caps[endpoint] * endpoint_host_counts[endpoint]
                          for endpoint in endpoint_host_counts) + idle_hosts * IDLE_WATTS_PER_NODE
    cluster_cap_increase = cluster_cap - allocated_power

    total_hosts = sum(endpoint_host_counts.values())
    cap_increase_per_host = cluster_cap_increase / total_hosts

    # Transform the per-job power caps to sum up to the cluster power cap
    # Expected to have an effect when a new job launches/finishes
    # and when the cluster cap changes.
    endpoint_power_caps.update({
        endpoint: endpoint_power_caps[endpoint] + cap_increase_per_host
        for endpoint in endpoint_power_caps})

    slack_power = {endpoint:
                   max(0, 0.95 * (endpoint_power_caps[endpoint]
                                  - endpoint_power_measurements[endpoint]) * endpoint_host_counts[endpoint])
                   for endpoint in endpoint_host_counts}
    total_slack_power = sum(slack_power.values())

    endpoints_under_pressure = [endpoint for endpoint in endpoint_host_counts
                                if slack_power[endpoint] == 0]
    total_hosts_under_pressure = sum(endpoint_host_counts[endpoint]
                                     for endpoint in endpoints_under_pressure)

    # Deallocate slack power from its current owning endpoints.
    new_power_caps = {
        endpoint: endpoint_power_caps[endpoint] - (
            slack_power[endpoint] / endpoint_host_counts[endpoint])
        for endpoint in endpoint_host_counts}

    # Distribute slack power across endpoints that are under pressure.
    new_power_caps.update({
        endpoint: new_power_caps[endpoint] + (
            total_slack_power / total_hosts_under_pressure)
        for endpoint in endpoints_under_pressure})

    # Uniformly distribute the difference across hosts (over or under)
    unallocated_power = cluster_cap - sum(
        new_power_caps[endpoint] * endpoint_host_counts[endpoint]
        for endpoint in endpoint_host_counts) - idle_hosts * IDLE_WATTS_PER_NODE
    new_power_caps.update({
        endpoint: new_power_caps[endpoint] + (
            unallocated_power / total_hosts)
        for endpoint in endpoint_host_counts})
    print('uniformly distribute', unallocated_power, 'then total is', sum(
        new_power_caps[endpoint]*endpoint_host_counts[endpoint]
        for endpoint in endpoint_host_counts))

    endpoint_power_caps.update(new_power_caps)


async def rebalance_jobs(reader, writer):
    global pre_balance_barrier
    global barrier_entries
    endpoint_addr = writer.get_extra_info('peername')[0]

    host_count_bytes = await reader.readline()
    host_count = int(host_count_bytes.decode())

    endpoint_power_bytes = await reader.readline()
    endpoint_power = float(endpoint_power_bytes.decode())
    initial_power_limit = float(280) # TODO: Could give NaN, then read-back what the applied limit was.
    print(f'{endpoint_addr}: {host_count} hosts, {endpoint_power} W')
    writer.write(f'{initial_power_limit}\n'.encode())
    await writer.drain()

    # Reset the barriers to wait on all connected endpoints
    async with barrier_entry_lock:
        endpoint_host_counts[endpoint_addr] = host_count
        endpoint_power_caps[endpoint_addr] = initial_power_limit

    # There's an instance of this loop per job
    while True:
        endpoint_power_bytes = await reader.readline()
        if not endpoint_power_bytes:
            break
        power_bytes = endpoint_power_bytes.decode()
        endpoint_power = float(power_bytes)
        endpoint_power_measurements[endpoint_addr] = endpoint_power
        print(f"Received {endpoint_power} from {endpoint_addr}")

        # Wait for all new samples to arrive before rebalancing the budget
        # Python 3.6 doesn't have a barrier implementation. Use a semaphore+lock instead
        async with barrier_entry_lock:
            barrier_entries += 1
            print('entries',barrier_entries, 'out of', len(endpoint_host_counts))
            if barrier_entries == len(endpoint_host_counts):
                # Make a single exit per barrier trigger a rebalancing calculation
                calculate_balance_targets()
                pre_balance_barrier.release()
        await pre_balance_barrier.acquire()
        pre_balance_barrier.release()
        async with barrier_entry_lock:
            barrier_entries -= 1
            if barrier_entries == 0:
                await pre_balance_barrier.acquire()


        new_power_target = endpoint_power_caps[endpoint_addr]
        print(f"New target for {endpoint_addr}: {new_power_target}")
        job_power_limit = f'{new_power_target}\n'

        writer.write(job_power_limit.encode())
        await writer.drain()
        await asyncio.sleep(0.25)

    async with barrier_entry_lock:
        del endpoint_power_measurements[endpoint_addr]
        del endpoint_host_counts[endpoint_addr]
        del endpoint_power_caps[endpoint_addr]
    print(f"Close the client {endpoint_addr} socket")
    writer.close()
    async with barrier_entry_lock:
        if barrier_entries == len(endpoint_host_counts) and len(endpoint_host_counts) > 0:
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
print('Serving on {}'.format(server.sockets[0].getsockname()))
try:
    loop.run_forever()
except KeyboardInterrupt:
    pass

# Close the server
server.close()
loop.run_until_complete(server.wait_closed())
loop.close()
