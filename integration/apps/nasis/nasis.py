#  Copyright (c) 2015 - 2021, Intel Corporation
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#      * Redistributions of source code must retain the above copyright
#        notice, this list of conditions and the following disclaimer.
#
#      * Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in
#        the documentation and/or other materials provided with the
#        distribution.
#
#      * Neither the name of Intel Corporation nor the names of its
#        contributors may be used to endorse or promote products derived
#        from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

'''
AppConf class for NAS IS.
'''

import os
import math

from apps import apps
from math import sqrt


def setup_run_args(parser):
    """ Add common arguments for all run scripts.
    """
    parser.add_argument('--npb-class', default='D')
    parser.add_argument('--ranks-per-node', dest='ranks_per_node',
                        action='store', type=int,
                        help='Number of physical cores to reserve for the app. '
                             'If not defined, all nodes but one will be reserved, '
                             'rounding down to a power of two and leaving one '
                             'core for GEOPM.')

def create_appconf(mach, args):
    ''' Create a NASISAppConf object from an ArgParse and experiment.machine object.
    '''
    return NASISAppConf(mach, args.npb_class, args.ranks_per_node)

class NASISAppConf(apps.AppConf):
    @staticmethod
    def name():
        return 'nas_is'

    def __init__(self, mach, npb_class, ranks_per_node):
        benchmark_dir = os.path.dirname(os.path.abspath(__file__))
        self._exec_path = os.path.join(benchmark_dir, "NPB3.4.2", "NPB3.4-MPI", "bin", "is." + npb_class + ".x")
        self._exec_args = []
        if ranks_per_node is None:
            # Leave one core for non-app work (e.g., geopm)
            max_cores = mach.num_core() - 1
        else:
            if ranks_per_node > mach.num_core():
                raise RuntimeError('The count of requested cores ({}) is more than the number of available ' +
                                   'cores ({})'.format(ranks_per_node, mach.num_core()))
            max_cores = ranks_per_node
        # The count of NPB integer sort cores must be a power of two
        self._ranks_per_node = 2 ** math.floor(math.log2(max_cores))

    def get_rank_per_node(self):
        return self._ranks_per_node

    def get_cpu_per_rank(self):
        return 1

    def parse_fom(self, log_path):
        with open(log_path) as fid:
            for line in fid.readlines():
                if line.strip().startswith('Mop/s total'):
                    total_ops_sec = float(line.split()[-1])
                    return total_ops_sec
        return float('nan')
