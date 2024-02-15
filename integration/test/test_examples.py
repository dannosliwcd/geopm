#!/usr/bin/env python3
#
#  Copyright (c) 2015 - 2024, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

import unittest
import os
import subprocess
from integration.test import util


class TestIntegrationExamples(unittest.TestCase):
    @util.skip_unless_batch()
    def test_monitor_pairs(self):
        '''
        Check that each host running the multi-host monitoring script creates a
        log file, and that the log files span times approximately equal to the
        requested tail length.
        '''
        monitor_script_path = os.path.join(
           os.path.dirname(
            os.path.dirname(
             os.path.dirname(
              os.path.realpath(__file__)))),
           'examples/monitor_pairs/monitor.py')
        destination_path = f'{os.path.realpath(__file__)}.trace-%hostname'
        tail_seconds = 1
        hosts = subprocess.run(['mpirun', '-n', '2', '-ppn', '1', 'hostname'],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=1,
                               check=True, universal_newlines=True).stdout.splitlines()

        subprocess.run(['mpirun', '-n', '2', '-ppn', '1', monitor_script_path,
                        f'--tail-seconds={tail_seconds}',
                        f'--destination-path={destination_path}',
                        f'--time={tail_seconds*2}',
                        '--period=0.02'],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       timeout=tail_seconds * 4)

        for host in hosts:
            host_path = destination_path.replace('%hostname', host)
            with open(host_path) as f:
                first_line = next(f)
                for last_line in f:
                    pass
            first_time = float(first_line.split(',', 1)[0])
            last_time = float(last_line.split(',', 1)[0])
            self.assertAlmostEqual(last_time - first_time, tail_seconds, delta=0.1,
                                   msg=f'{host_path} should have a {tail_seconds}-second tail.')


if __name__ == '__main__':
    unittest.main()
