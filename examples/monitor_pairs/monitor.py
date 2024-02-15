#!/usr/bin/env python3
import argparse
import subprocess
import os
import sys
import io
import itertools
import socket
import time
from geopmdpy import pio, topo
from geopmdpy.session import Session
from string import Template

class TracePathTemplate(Template):
    delimiter = '%'

HOME = os.getenv('HOME', '.')
PBS_NODEFILE = os.getenv('PBS_NODEFILE')
SLURM_JOB_NODELIST = os.getenv('SLURM_JOB_NODELIST')
HOSTNAME = socket.gethostname()

HOSTNAME_PATH_KEY = 'hostname'
TIMESTAMP_PATH_KEY = 'timestamp'

def get_nodes_in_current_job():
    if PBS_NODEFILE is not None:
        with open(PBS_NODEFILE) as f:
            return set(node.strip() for node in f.readlines())
    elif SLURM_JOB_NODELIST is not None:
        return set(subprocess.run(['scontrol', 'show', 'hostnames', SLURM_JOB_NODELIST],
                                  stdout=subprocess.PIPE, universal_newlines=True).stdout.splitlines())
    else:
        return set()


def get_signal_requests():
    # Default to all alias signals
    requests = [('TIME', 'board', '0')]
    monitor_signals = [signal for signal in pio.signal_names()
                       if '::' not in signal
                       and signal != 'TIME']

    for signal_name in monitor_signals:
        domain_type = topo.domain_name(pio.signal_domain_type(signal_name))
        for domain_idx in range(topo.num_domain(domain_type)):
            requests.append((signal_name, domain_type, str(domain_idx)))
    return requests


if __name__ == '__main__':
    if HOSTNAME is None:
        print('Error: Unknown hostname.', file=sys.stderr)
        sys.exit(1)
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--time', dest='time', type=float, default=9e9,
                        help='Elapsed time after which the tracer stops running, in seconds. Default %(default)s')
    parser.add_argument('-p', '--period', dest='period', type=float, default=0.1,
                        help='Period at which platform metrics are sampled, in seconds. '
                             'Default: %(default)s')
    parser.add_argument('--pid', type=int,
                        help='Stop tracing when the given process PID ends')
    parser.add_argument('--destination-host',
                        help='Logging destination hostname or ssh alias')
    parser.add_argument('--destination-path',
                        default=f'{HOME}/geopm_monitor.trace-%{HOSTNAME_PATH_KEY}-%{TIMESTAMP_PATH_KEY}',
                        help='Path to the destination log file in the destination host. '
                             f'%%{HOSTNAME_PATH_KEY} will be replaced with the name of the '
                             f'host that generated the file. %%{TIMESTAMP_PATH_KEY} will be '
                             'replaced with the seconds since the epoch (unix timestamp) '
                             'on the source node at the start of logging. Default: %(default)s')
    tail_group = parser.add_mutually_exclusive_group()
    tail_group.add_argument('--tail-seconds',
                            type=float,
                            default=10.0,
                            help='Store only the tail of the trace with up to TAIL_SECONDS seconds '
                                 'of trace samples.')
    tail_group.add_argument('--full-trace',
                            action='store_true',
                            help='Store the full trace instead of only saving the tail.')
    args = parser.parse_args()

    session_requests = get_signal_requests()

    if args.destination_host is None:
        nodes = sorted(get_nodes_in_current_job())
        if len(nodes) < 2:
            print('Error: Must have at least two nodes for one node to monitor another.',
                  file=sys.stderr)
            sys.exit(1)

        for source_host, destination_host in zip(nodes, itertools.islice(itertools.cycle(nodes), 1, None)):
            if source_host.startswith(HOSTNAME):
                break
        if source_host != HOSTNAME:
            print(f'Error: Did not find {HOSTNAME} in the node list of the current job', file=sys.stderr)
            sys.exit(1)
    else:
        source_host = HOSTNAME
        destination_host = args.destination_host

    destination_path = TracePathTemplate(args.destination_path).safe_substitute({
        HOSTNAME_PATH_KEY: HOSTNAME,
        TIMESTAMP_PATH_KEY: time.time(),
    })

    print(f'{source_host} -> {destination_host}:{destination_path}')

    if args.full_trace:
        remote_command = f'/usr/bin/cat > {destination_path}'
    else:
        # Print the header to separate file since the tail won't contain headers
        header_names = [f'"{name}-{domain_name}-{domain_idx}"'
                        if domain_name != 'board' else f'"{name}"'
                        for name, domain_name, domain_idx in session_requests]
        ssh_proc = subprocess.Popen(
            ['ssh', destination_host, '-q', '-T', f'/usr/bin/cat > {destination_path}.header'],
            stdin=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
        )
        print(','.join(header_names), file=ssh_proc.stdin)
        ssh_proc.stdin.close()
        if ssh_proc.wait(timeout=1.0) != 0:
            print(f'Error: Failed to write header to {destination_host}:{destination_path}.header. '
                  f'Reason: {ssh_proc.stderr.read()}', file=sys.stderr)
            sys.exit(1)

        tail_lines = int(args.tail_seconds // args.period)
        remote_command = f'/usr/bin/tail -n {tail_lines} > {destination_path}'

    ssh_proc = subprocess.Popen(
        ['ssh', destination_host, '-q', '-T', remote_command],
        stdin=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
        bufsize=1,  # Line-buffered
    )

    request_stream = io.StringIO('\n'.join(f'{name} {domain_name} {domain_idx}'
                                           for name, domain_name, domain_idx in session_requests))
    session = Session()
    try:
        session.run(run_time=args.time,
                    period=args.period,
                    pid=args.pid,
                    print_header=True,
                    request_stream=request_stream,
                    out_stream=ssh_proc.stdin,
                    )
    except KeyboardInterrupt:
        pass
    ssh_proc.stdin.close()
    if ssh_proc.wait(timeout=1.0) != 0:
        print(f'Error: Failed to write log to {destination_host}:{destination_path}. '
              f'Reason: {ssh_proc.stderr.read()}', file=sys.stderr)
        sys.exit(1)
