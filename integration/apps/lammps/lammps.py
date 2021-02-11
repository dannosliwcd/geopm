#  Copyright (c) 2015, 2016, 2017, 2018, 2019, 2020, Intel Corporation
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
AppConf class for LAMMPS benchmarks.
'''

import os
import re
import shlex
import distutils.dir_util

from apps import apps


def setup_run_args(parser):
    """ Add common arguments for all run scripts.
    """
    parser.add_argument('--lammps-input', dest='lammps_input',
                        action='store', type=str,
                        default='lammps-stable_29Oct2020/bench/in.rhodo.scaled',
                        help='Path to the input file (see .in.* files in the bench' +
                             'directory of the LAMMPS source tarball). ' +
                             'Absolute path or relative to the app ' +
                             'directory. Default is in.rhodo.scaled')
    parser.add_argument('--lammps-cores-per-node', dest='lammps_cores_per_node',
                        action='store', type=int,
                        help='Number of physical cores to reserve for the app. ' +
                             'If not defined, the highest even number of cores less than ' +
                             'the total number in the node will be reserved (leaving at ' +
                             'least one core for GEOPM).')
    parser.add_argument('--lammps-cores-per-rank', dest='lammps_cores_per_rank',
                        action='store', type=int, default=1,
                        help='Number of physical cores to use per rank for OMP parallelism.' +
                             'Default is 1.')


def create_appconf(mach, args):
    ''' Create a LammpsAppConfig object from an argparse and experiment.machine object.
    '''
    return LammpsAppConf(mach, args.lammps_input, args.lammps_cores_per_node,
                         args.lammps_cores_per_rank)


class LammpsAppConf(apps.AppConf):
    DEFAULT_PROBLEM_SIZES = {
        'in.rhodo.scaled': ['-v', 'x', '7',
                            '-v', 'y', '7',
                            '-v', 'z', '7'],
    }

    @staticmethod
    def name():
        return 'lammps'

    def __init__(self, mach, problem_file, cores_per_node, cores_per_rank):
        benchmark_dir = os.path.dirname(os.path.abspath(__file__))
        exec_dir = 'lammps-stable_29Oct2020/build/lmp'
        input_dir = 'lammps-stable_29Oct2020/bench'

        self._exec_path = os.path.join(benchmark_dir, exec_dir)
        self._input_dir = os.path.join(benchmark_dir, input_dir)

        exec_args = list()

        problem_basename = os.path.basename(problem_file)
        exec_args.extend(['-in', problem_basename])

        if problem_basename in self.DEFAULT_PROBLEM_SIZES:
            exec_args.extend(self.DEFAULT_PROBLEM_SIZES[problem_basename])

        # Recommended args come from https://lammps.sandia.gov/doc/Speed_intel.html
        exec_args.extend(['-log', 'none',
                          '-pk', 'intel', '0',
                          'omp', str(cores_per_rank - 1),
                          'lrt', 'yes',
                          '-sf', 'intel',
                          ])

        self._exec_args = ' '.join(shlex.quote(arg) for arg in exec_args)

        if cores_per_node is None:
            # Reserve one for geopm.
            reserved_cores = mach.num_core() - 1
            # If odd number throw one out.
            reserved_cores = (reserved_cores // 2) * 2
            self._cores_per_node = reserved_cores
        else:
            if cores_per_node > mach.num_core():
                raise RuntimeError('Number of requested cores is more than the number ' +
                                   'of available cores: {}'.format(cores_per_node))
            self._cores_per_node = cores_per_node
        if self._cores_per_node % cores_per_rank != 0:
            raise RuntimeError('Number of requested cores is not divisible by OMP threads '
                               'per rank: {} % {}'.format(self._cores_per_node, cores_per_rank))
        self._cores_per_rank = cores_per_rank

    def get_rank_per_node(self):
        return self._cores_per_node // self.get_cpu_per_rank()

    def get_cpu_per_rank(self):
        return self._cores_per_rank

    def trial_setup(self, run_id, output_dir):
        # Unlike shutil, this copies the contents of the source dir, without
        # the source dir itself
        distutils.dir_util.copy_tree(self._input_dir, output_dir)

    def get_custom_geopm_args(self):
        return ['--geopm-hyperthreads-disable']

    def parse_fom(self, log_path):
        with open(log_path) as fid:
            print('reading', log_path)
            # for line in fid.readlines():
            #     float_regex = r'([-+]?(\d+(\.\d*)?|\.\d+)([eE][-+]?\d+)?)'
            #     m_zones = re.match(r'^Zones:\s*' + float_regex, line)
            #     m_runtime = re.match(r'^hydro cycle run time=\s*' +
            #                          float_regex, line)
            #     m_cycle = re.match(r'^cycle\s*=\s*' + float_regex, line)
            #     if m_zones:
            #         zones = int(m_zones.group(1))
            #     if m_runtime:
            #         runtime = float(m_runtime.group(1))
            #     if m_cycle:
            #         cycle = int(m_cycle.group(1))
        return 1 / 1 #runtime
