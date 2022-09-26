#  Copyright (c) 2015 - 2022, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

'''
AppConf class for NAS LU-MZ.
'''

import os

from apps import apps
from math import sqrt


def setup_run_args(parser):
    """ Add common arguments for all run scripts.
    """
    parser.add_argument('--npb-class', default='C')
    parser.add_argument('--ranks-per-node', dest='ranks_per_node',
                        action='store', type=int,
                        help='Number of physical cores to reserve for the app. '
                             'If not defined, all nodes but one will be reserved (leaving one '
                             'node for GEOPM).')

def create_appconf(mach, args):
    ''' Create a NASLUMZAppConf object from argparse arguments and an
    experiment.machine object.
    '''
    return NASLUMZAppConf(mach, args.npb_class, args.ranks_per_node, args.node_count)

class NASLUMZAppConf(apps.AppConf):
    @staticmethod
    def name():
        return 'nas_lu'

    def __init__(self, mach, npb_class, ranks_per_node, node_count):
        benchmark_dir = os.path.dirname(os.path.abspath(__file__))
        self._exec_path = os.path.join(
            benchmark_dir, "NPB3.4.1-MZ", "NPB3.4-MZ-MPI", "bin", "lu-mz." + npb_class + ".x")
        self._exec_args = []
        self._core_count = mach.num_core()
        if ranks_per_node is None:
            self._ranks_per_node = mach.num_package() * 2
        else:
            if ranks_per_node > self._core_count:
                raise RuntimeError('Number of requested cores is more than the number of available ' +
                                   'cores: {} vs. {}'.format(ranks_per_node, self._core_count))
            self._ranks_per_node = ranks_per_node

    def get_rank_per_node(self):
        return self._ranks_per_node

    def get_cpu_per_rank(self):
        thread_count = (self._core_count // self._ranks_per_node) // 2 * 2
        thread_count = max(1, thread_count)
        return thread_count

    def parse_fom(self, log_path):
        with open(log_path) as fid:
            for line in fid.readlines():
                if line.strip().startswith('Mop/s total'):
                    total_ops_sec = float(line.split()[-1])
                    return total_ops_sec
        return float('nan')
