#!/usr/bin/env python3
import asyncio
import os
import sys
GEOPM_ENDPOINT_SOCKET = 63094  # should be configurable
#                      (GEOPM)
GEOPM_ENDPOINT_SERVER_HOST = os.getenv('GEOPM_ENDPOINT_SERVER_HOST')
if GEOPM_ENDPOINT_SERVER_HOST is None:
    print('Error: Must set env var GEOPM_ENDPOINT_SERVER_HOST', file=sys.stderr)
    sys.exit(1)


async def tcp_policy_sample(argv):
    reader, writer = await asyncio.open_connection(
        GEOPM_ENDPOINT_SERVER_HOST, GEOPM_ENDPOINT_SOCKET)
    policy = ''

    print('Starting', argv)

    host_count = 2
    average_host_power = 100.0
    writer.write(f'{host_count}\n{average_host_power}\n'.encode())
    await writer.drain()

    # while endpoint is running:
    for i in range(5):
        policy_bytes = await reader.readline()
        if not policy_bytes:
            break
        policy = policy_bytes.decode()
        power_cap = float(policy)
        print(f'Received power cap: {power_cap}')

        average_host_power = min(100, power_cap)
        power_message = f'{average_host_power}\n'
        writer.write(power_message.encode())
        await writer.drain()

    print('All done!')
    writer.close()


wrapped_argv = ['geopmlaunch', 'srun', '...']
loop = asyncio.get_event_loop()
loop.run_until_complete(tcp_policy_sample(wrapped_argv))
loop.close()
