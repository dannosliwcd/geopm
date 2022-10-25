#  Copyright (c) 2015 - 2022, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

'''
AppConf class for NAS BT-MZ.
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
    ''' Create a ArithmeticIntensityAppConf object from an ArgParse and experiment.machine object.
    '''
    return NASBTMZAppConf(mach, args.npb_class, args.ranks_per_node)

class NASBTMZAppConf(apps.AppConf):
    @staticmethod
    def name():
        return 'nas_bt'

    def __init__(self, mach, npb_class, ranks_per_node):
        benchmark_dir = os.path.dirname(os.path.abspath(__file__))
        self._exec_path = os.path.join(benchmark_dir, "NPB3.4.1-MZ", "NPB3.4-MZ-MPI", "bin", "bt-mz." + npb_class + ".x")
        self._exec_args = []
        if ranks_per_node is None:
            self._ranks_per_node = mach.num_core() - 1
        else:
            if ranks_per_node > mach.num_core():
                raise RuntimeError('Number of requested cores is more than the number of available ' +
                                   'cores: {} vs. {}'.format(ranks_per_node, mach.num_core()))
            self._ranks_per_node = ranks_per_node

    def get_rank_per_node(self):
        return self._ranks_per_node

    def get_cpu_per_rank(self):
        return 1

    def get_custom_geopm_args(self):
        args = ['--geopm-ompt-disable']
        return args

    def parse_fom(self, log_path):
        with open(log_path) as fid:
            for line in fid.readlines():
                if line.strip().startswith('Mop/s total'):
                    total_ops_sec = float(line.split()[-1])
                    return total_ops_sec
        return float('nan')
