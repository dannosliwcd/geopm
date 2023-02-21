#!/usr/bin/env python3
import asyncio
import os
import sys

GEOPM_ENDPOINT_SOCKET = 63094  # should be configurable
GEOPM_ENDPOINT_SERVER_HOST = os.getenv('GEOPM_ENDPOINT_SERVER_HOST')
if GEOPM_ENDPOINT_SERVER_HOST is None:
    print('Error: Must set env var GEOPM_ENDPOINT_SERVER_HOST', file=sys.stderr)
    sys.exit(1)

endpoint_power_measurements = dict()
endpoint_power_caps = dict()


def get_cluster_power_target():
    return 200


async def get_endpoint_power_target(endpoint_addr):
    #      -> Also rate-limit it ~1sec?
    await asyncio.sleep(1)
    return 122.0


async def rebalance_jobs(reader, writer):
    endpoint_addr = writer.get_extra_info('peername')[0]

    host_count_bytes = await reader.readline()
    host_count = int(host_count_bytes.decode())
    endpoint_power_bytes = await reader.readline()
    endpoint_power = float(endpoint_power_bytes.decode())
    initial_power_limit = 200.0
    print(f'{endpoint_addr}: {host_count} hosts, {endpoint_power} W')
    writer.write(f'{initial_power_limit}\n'.encode())
    await writer.drain()

    while True:
        endpoint_power_bytes = await reader.readline()
        if not endpoint_power_bytes:
            break
        sample = endpoint_power_bytes.decode()
        endpoint_power = float(sample)
        endpoint_power_measurements[endpoint_addr] = endpoint_power
        print(f"Received {endpoint_power} from {endpoint_addr}")

        new_power_target = await get_endpoint_power_target(endpoint_addr)
        job_power_limit = f'{new_power_target}\n'
        writer.write(job_power_limit.encode())
        await writer.drain()

    print(f"Close the client {endpoint_addr} socket")
    writer.close()


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
