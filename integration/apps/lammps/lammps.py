#  Copyright (c) 2015 - 2022, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

'''
AppConf for LAMMPS benchmarks.
'''

import os
import re
import shlex
import distutils.dir_util
import shutil

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
        'water_collapse.lmp': [],
    }

    @staticmethod
    def name():
        return 'lammps'

    def __init__(self, mach, problem_file, cores_per_node, cores_per_rank):
        benchmark_dir = os.path.dirname(os.path.abspath(__file__))
        exec_dir = 'lammps-stable_29Oct2020/src/lmp_intel_cpu_intelmpi'
        input_dir = 'lammps-stable_29Oct2020/bench'

        self._exec_path = os.path.join(benchmark_dir, exec_dir)
        self._input_dir = os.path.join(benchmark_dir, input_dir)
        self._example_dir = os.path.join(benchmark_dir, 'lammps-stable_29Oct2020/examples')

        exec_args = list()

        problem_basename = os.path.basename(problem_file)
        exec_args.extend(['-in', problem_basename])

        if problem_basename in self.DEFAULT_PROBLEM_SIZES:
            exec_args.extend(self.DEFAULT_PROBLEM_SIZES[problem_basename])

        exec_args.extend(['-log', 'none',
                          '-pk', 'intel', '0',
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
        distutils.dir_util.copy_tree(os.path.join(self._example_dir, 'USER', 'sph', 'water_collapse'), output_dir)

    def parse_fom(self, log_path):
        with open(log_path) as fid:
            for line in fid.readlines():
                perf_regex = r'Performance:.*[^0-9.]([0-9.]+) timesteps/s'
                m_perf = re.match(perf_regex, line)
                if m_perf:
                    steps_per_second = float(m_perf.group(1))
                    return steps_per_second
        return float('nan')
