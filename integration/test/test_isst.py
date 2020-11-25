#!/usr/bin/env python
#
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

from __future__ import absolute_import

import os
import sys
import unittest
import shlex
import subprocess
import io
import json

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import geopm_context
import geopmpy.io
import util
import geopmpy.topo
import geopmpy.pio
import geopm_test_launcher
import random

# Check running as root

class TestIntegrationISST(unittest.TestCase):

    # TODO: Replace with commented system calls--need to update status to "enable"/"disable" when you do so
    def set_turbo_freq(self, status):
        num_pkg = geopmpy.topo.num_domain('package')
        for pkg in range(num_pkg):
            geopmpy.pio.write_control("SST::TURBO_ENABLE:ENABLE", "package", pkg, status)
        #sst_comm=["intel-speed-select", "-f", "json", "turbo-freq", status, "-l", "0"]
        #sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        #self.assertEqual(0, sst_comm_ret.returncode)


    # TODO: Replace with commented system calls--need to update status to "enable"/"disable" when you do so
    def set_core_power(self, status):
        num_pkg = geopmpy.topo.num_domain('package')
        for pkg in range(num_pkg):
            if status == 0:
                geopm_tf_status = geopmpy.pio.read_signal("SST::TURBO_ENABLE:ENABLE", "package", pkg)
                if geopm_tf_status == 1:
                    self.set_turbo_freq(0)
            geopmpy.pio.write_control("SST::COREPRIORITY_ENABLE:ENABLE", "package", pkg, status)
        #sst_comm=["intel-speed-select", "-f", "json", "core-power", status]
        #sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        #self.assertEqual(0, sst_comm_ret.returncode)
   

    def setUp(self):
        #self.set_turbo_freq(0)
        #self.set_core_power(0)
        pass


    def tearDown(self):
        pass

    def get_turbo_freq_status(self):
        pass

    def get_sst_corepower_assoc(self, core_idx):
        sst_comm=["intel-speed-select", "-f", "json", "-c", str(core_idx), "core-power", "get-assoc"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        sst_comm_json=json.loads(sst_comm_ret.stderr)
        for key in sst_comm_json:
            sst_assoc=int(sst_comm_json[key]['get-assoc']['clos'])
        return sst_assoc

    def get_sst_corepower_config(self, pkg_id, clos_id):
        num_pkg = geopmpy.topo.num_domain('package')
        num_core = geopmpy.topo.num_domain('core')
        core_idx=int(pkg_id*(num_core/num_pkg))

        sst_comm=["intel-speed-select", "-f", "json", "core-power", "get-config", "-c", str(clos_id)]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        sst_comm_json=json.loads(sst_comm_ret.stderr)
        sst_idx="package-" + str(pkg_id) + ":die-0:cpu-" + str(core_idx)
        min_freq = sst_comm_json[sst_idx]["core-power"]["clos-min"].replace(" MHz", "")
        max_freq = sst_comm_json[sst_idx]["core-power"]["clos-max"].replace(" MHz", "")

        sst_cp = {}
        sst_cp['weight'] = int(sst_comm_json[sst_idx]["core-power"]["clos-proportional-priority"])
        sst_cp['min'] = 1E6 * int(sst_comm_json[sst_idx]["core-power"]["clos-min"].replace(" MHz", ""))
        sst_cp['max'] = 1E6 * int(sst_comm_json[sst_idx]["core-power"]["clos-max"].replace(" MHz", ""))
        return sst_cp

    def geopm_get_corepower_config(self, pkg_id, clos_id):
        geopm_signal_name = "SST::COREPRIORITY_" + str(clos_id)
        geopm_cp = {}
        geopm_cp['weight'] = int(geopmpy.pio.read_signal(geopm_signal_name + ":WEIGHT", "package", pkg_id))
        geopm_cp['min'] = int(geopmpy.pio.read_signal(geopm_signal_name + ":FREQUENCY_MIN", "package", pkg_id))
        geopm_cp['max'] = int(geopmpy.pio.read_signal(geopm_signal_name + ":FREQUENCY_MAX", "package", pkg_id))
        return geopm_cp

    def test_read_iss_config(self):
        sst_comm=["intel-speed-select", "-f", "json", "perf-profile", "get-config-levels"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        sst_comm_json=json.loads(sst_comm_ret.stderr)

        num_pkg = geopmpy.topo.num_domain('package')
        num_core = geopmpy.topo.num_domain('core')

        for pkg in range(num_pkg):
            core_idx=int(pkg*(num_core/num_pkg))
            sst_idx="package-" + str(pkg) + ":die-0:cpu-" + str(core_idx)
            sst_config_level=sst_comm_json[sst_idx]["get-config-levels"]

            geopm_config_level = geopmpy.pio.read_signal("SST::CONFIG_LEVEL:LEVEL", "package", pkg)
            self.assertEqual(int(sst_config_level), int(geopm_config_level))

    def test_read_turbofreq_support(self):
        sst_comm=["intel-speed-select", "--info"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)

        sst_tf_support = 0
        for line in (str)(sst_comm_ret.stderr).split('\n'):
            if 'SST-TF' in line:
                if 'is supported' in line:
                    sst_tf_support = 1

        geopm_tf_support = geopmpy.pio.read_signal("SST::TURBOFREQ_SUPPORT:SUPPORTED", "package", 0)
        self.assertEqual(sst_tf_support, geopm_tf_support)

    def test_read_cp_support(self):


        num_pkg = geopmpy.topo.num_domain('package')
        num_core = geopmpy.topo.num_domain('core')

        sst_comm=["intel-speed-select", "-f", "json", "core-power", "info"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        sst_comm_json=json.loads(sst_comm_ret.stderr)

        for pkg in range(num_pkg):
            core_idx=int(pkg*(num_core/num_pkg))
            sst_idx="package-" + str(pkg) + ":die-0:cpu-" + str(core_idx)
            sst_cp_support=sst_comm_json[sst_idx]["core-power"]["support-status"]
            if sst_cp_support == "supported":
                sst_cp_support=1
            else:
                sst_cp_support=0

            geopm_cp_support = geopmpy.pio.read_signal("SST::COREPRIORITY_SUPPORT:CAPABILITIES","package", pkg)
            self.assertEqual(int(sst_cp_support), int(geopm_cp_support))

    def test_turbofreq_status(self):
        num_pkg = geopmpy.topo.num_domain('package')
        for pkg in range(num_pkg):
            #Check read in each state
            turbofreq_states = ["disable", "enable"]
            for st_idx in range(len(turbofreq_states)):

                sst_comm=["intel-speed-select", "-f", "json", "turbo-freq", turbofreq_states[st_idx], "-l", "0"]
                sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
                self.assertEqual(0, sst_comm_ret.returncode)

                turbofreq_status = geopmpy.pio.read_signal("SST::TURBO_ENABLE:ENABLE", "package", pkg)
                self.assertEqual(st_idx, int(turbofreq_status))

            #Check write in each state
            for begin_status in ["disable", "enable"]:
                for toggle_status in range(2):
                    sst_comm=["intel-speed-select", "-f", "json", "turbo-freq", begin_status, "-l", str(pkg)]
                    sst_comm_ret=subprocess.run(sst_comm, capture_output=True)

                    self.assertEqual(0, sst_comm_ret.returncode)
                    geopmpy.pio.write_control("SST::TURBO_ENABLE:ENABLE", "package", pkg, toggle_status)

                    turbofreq_status = geopmpy.pio.read_signal("SST::TURBO_ENABLE:ENABLE", "package", pkg)
                    self.assertEqual(toggle_status, int(turbofreq_status))

    def test_cp_status(self):

        num_pkg = geopmpy.topo.num_domain('package')
        num_core = geopmpy.topo.num_domain('core')

        for pkg in range(num_pkg):
            core_idx=int(pkg*(num_core/num_pkg))

            self.set_core_power(0)
            geopm_cp_status = geopmpy.pio.read_signal("SST::COREPRIORITY_ENABLE:ENABLE","package", pkg)
            self.assertEqual(0.0, geopm_cp_status)

            self.set_core_power(1)
            geopm_cp_status = geopmpy.pio.read_signal("SST::COREPRIORITY_ENABLE:ENABLE","package", pkg)
            self.assertEqual(1.0, geopm_cp_status)

            # Test Write
            state_list=[1.0, 0.0, 1.0]
            for state_idx in state_list:
                geopmpy.pio.write_control("SST::COREPRIORITY_ENABLE:ENABLE", "package", pkg, state_idx)
                geopm_cp_status = geopmpy.pio.read_signal("SST::COREPRIORITY_ENABLE:ENABLE","package", pkg)
                self.assertEqual(state_idx, geopm_cp_status)


    def test_read_hp_ncores(self):
        sst_comm=["intel-speed-select", "-f", "json", "turbo-freq", "info", "-l", "0"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        sst_comm_json=json.loads(sst_comm_ret.stderr)

        num_pkg = geopmpy.topo.num_domain('package')
        num_core = geopmpy.topo.num_domain('core')

        for pkg in range(num_pkg):
            core_idx=int(pkg*(num_core/num_pkg))
            #TODO: Get dynamically # buckets supported
            for bucket_idx in range(3):
                sst_idx="package-" + str(pkg) + ":die-0:cpu-" + str(core_idx)
                sst_ncore_bucket=sst_comm_json[sst_idx]["speed-select-turbo-freq-properties"]["bucket-" + str(bucket_idx)]["high-priority-cores-count"]
                geopm_ncore_bucket = geopmpy.pio.read_signal("SST::HIGHPRIORITY_NCORES:"
                             + str(bucket_idx), "package", pkg)

                self.assertEqual(int(sst_ncore_bucket), int(geopm_ncore_bucket))

    def test_read_hp_trl(self):
        sst_comm=["intel-speed-select", "-f", "json", "turbo-freq", "info", "-l", "0"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        sst_comm_json=json.loads(sst_comm_ret.stderr)

        num_pkg = geopmpy.topo.num_domain('package')
        num_core = geopmpy.topo.num_domain('core')

        for pkg in range(num_pkg):
            core_idx=int(pkg*(num_core/num_pkg))
            #TODO: Get dynamically # buckets supported
            #      Use a try catch on sst_trls? catch set to 0
            for bucket_idx in range(3):
                sst_idx="package-" + str(pkg) + ":die-0:cpu-" + str(core_idx)
                sst_trls = sst_comm_json[sst_idx]["speed-select-turbo-freq-properties"]["bucket-"
                             + str(bucket_idx)]
                sst_sse_trl=1e6*int(sst_trls["high-priority-max-frequency(MHz)"])
                sst_avx_trl=1e6*int(sst_trls["high-priority-max-avx2-frequency(MHz)"])
                sst_avx3_trl=1e6*int(sst_trls["high-priority-max-avx512-frequency(MHz)"])

                geopm_sse_trl = geopmpy.pio.read_signal("SST::HIGHPRIORITY_FREQUENCY_SSE:"
                             + str(bucket_idx), "package", pkg)
                geopm_avx_trl = geopmpy.pio.read_signal("SST::HIGHPRIORITY_FREQUENCY_AVX2:"
                             + str(bucket_idx), "package", pkg)
                geopm_avx3_trl = geopmpy.pio.read_signal("SST::HIGHPRIORITY_FREQUENCY_AVX512:"
                             + str(bucket_idx), "package", pkg)

                self.assertEqual(int(sst_sse_trl), int(geopm_sse_trl))
                self.assertEqual(int(sst_avx_trl), int(geopm_avx_trl))
                self.assertEqual(int(sst_avx3_trl), int(geopm_avx3_trl))

    def test_read_lp_trl(self):
        sst_comm=["intel-speed-select", "-f", "json", "turbo-freq", "info", "-l", "0"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        sst_comm_json=json.loads(sst_comm_ret.stderr)

        num_pkg = geopmpy.topo.num_domain('package')
        num_core = geopmpy.topo.num_domain('core')

        for pkg in range(num_pkg):
            core_idx=int(pkg*(num_core/num_pkg))

            lp_trl=sst_comm_json["package-" + str(pkg) + ":die-0:cpu-" 
                    + str(core_idx)]["speed-select-turbo-freq-properties"]["speed-select-turbo-freq-clip-frequencies"]
            sst_sse_trl=1e6*int(lp_trl["low-priority-max-frequency(MHz)"])
            sst_avx_trl=1e6*int(lp_trl["low-priority-max-avx2-frequency(MHz)"])
            sst_avx3_trl=1e6*int(lp_trl["low-priority-max-avx512-frequency(MHz)"])
            geopm_sse_trl = geopmpy.pio.read_signal("SST::LOWPRIORITY_FREQUENCY:SSE", "package", pkg)
            geopm_avx_trl = geopmpy.pio.read_signal("SST::LOWPRIORITY_FREQUENCY:AVX2", "package", pkg)
            geopm_avx3_trl = geopmpy.pio.read_signal("SST::LOWPRIORITY_FREQUENCY:AVX512", "package", pkg)
                
            self.assertEqual(int(sst_sse_trl), int(geopm_sse_trl))
            self.assertEqual(int(sst_avx_trl), int(geopm_avx_trl))
            self.assertEqual(int(sst_avx3_trl), int(geopm_avx3_trl))

    def test_read_core_power_assoc(self):
        num_core = geopmpy.topo.num_domain('core')

        for core_idx in range(num_core):
            sst_assoc = self.get_sst_corepower_assoc(core_idx)
            geopm_assoc = int(geopmpy.pio.read_signal("SST::COREPRIORITY_ASSOCIATION", "core", core_idx))
            self.assertEqual(sst_assoc, geopm_assoc)

    def test_write_core_power_assoc(self):
        num_core = geopmpy.topo.num_domain('core')

        for core_idx in range(num_core):
            rand_clos = random.randint(0, 3) 
            geopmpy.pio.write_control("SST::COREPRIORITY_ASSOCIATION", "core", core_idx, rand_clos)
            sst_assoc = self.get_sst_corepower_assoc(core_idx)
            self.assertEqual(rand_clos, sst_assoc)
           
    def test_read_core_power_config(self):
        rand_clos = random.randint(0, 3) 
        cp_weight = random.randint(0,15)
        #Frequencies in 100 MHz
        #TODO: Do not hardcode frequency range
        cp_minfreq = random.randint(10, 35)
        cp_maxfreq = random.randint(cp_minfreq, 40)
   
        num_pkg = geopmpy.topo.num_domain('package')
        num_core = geopmpy.topo.num_domain('core')

        for pkg in range(num_pkg):
            sst_comm=["intel-speed-select", "-f", "json", "core-power", "config", "-c", str(rand_clos), "-w", str(cp_weight), "-n", str(cp_minfreq*100), "-m", str(cp_maxfreq*100)]
            sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
            self.assertEqual(0, sst_comm_ret.returncode)

            sst_config = self.get_sst_corepower_config(pkg, rand_clos)

            geopm_config = self.geopm_get_corepower_config(pkg, rand_clos)
    
            self.assertEqual(sst_config['weight'], geopm_config['weight'])
            self.assertEqual(sst_config['min'], geopm_config['min'])
            self.assertEqual(sst_config['max'], geopm_config['max'])

    def test_write_core_power_config(self):
   
        num_pkg = geopmpy.topo.num_domain('package')
        num_core = geopmpy.topo.num_domain('core')

        for pkg in range(num_pkg):
            rand_clos = random.randint(0, 3) 
            cp_weight = random.randint(0,15)
            #TODO: Do not hardcode frequency range
            cp_minfreq = random.randint(10, 35)
            cp_maxfreq = random.randint(cp_minfreq, 40)

            geopm_signal_name = "SST::COREPRIORITY_" + str(rand_clos)
            geopmpy.pio.write_control(geopm_signal_name + ":WEIGHT", "package", pkg, cp_weight)
            geopmpy.pio.write_control(geopm_signal_name + ":FREQUENCY_MIN", "package", pkg, 1e8 * cp_minfreq)
            geopmpy.pio.write_control(geopm_signal_name + ":FREQUENCY_MAX", "package", pkg, 1e8 * cp_maxfreq)

            sst_config = self.get_sst_corepower_config(pkg, rand_clos)
    
            self.assertEqual(cp_weight, sst_config['weight'])
            self.assertEqual(1E8 * cp_minfreq, sst_config['min'])
            self.assertEqual(1E8 * cp_maxfreq, sst_config['max'])

    
    def test_write_core_power_weight(self):
        num_pkg = geopmpy.topo.num_domain('package')

        for pkg in range(num_pkg):
            rand_clos = random.randint(0, 3) 
            cp_weight = random.randint(0,15)
            
            sst_init_config = self.get_sst_corepower_config(pkg, rand_clos)

            geopm_signal_name = "SST::COREPRIORITY_" + str(rand_clos)
            geopmpy.pio.write_control("SST::COREPRIORITY_" + str(rand_clos) + ":WEIGHT", "package", pkg, cp_weight)

            sst_post_config = self.get_sst_corepower_config(pkg, rand_clos)
    
            self.assertEqual(cp_weight, sst_post_config['weight'])
            self.assertEqual(sst_init_config['min'], sst_post_config['min'])
            self.assertEqual(sst_init_config['max'], sst_post_config['max'])

    def test_write_core_power_minfreq(self):
        pass
    def test_write_core_power_maxfreq(self):
        pass


if __name__ == '__main__':
    unittest.main()
