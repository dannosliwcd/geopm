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

    def get_turbo_freq_status(self):
        pass

    def enable_turbo_freq(self):
        sst_comm=["intel-speed-select", "-f", "json", "turbo-freq", "enable", "-l", "0"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        self.assertEqual(0, sst_comm_ret.returncode)
        pass

    def disable_turbo_freq(self):
        sst_comm=["intel-speed-select", "-f", "json", "turbo-freq", "disable", "-l", "0"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        self.assertEqual(0, sst_comm_ret.returncode)

    def enable_core_power(self):
        sst_comm=["intel-speed-select", "-f", "json", "core-power", "enable"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        self.assertEqual(0, sst_comm_ret.returncode)
    
    def disable_core_power(self):
        sst_comm=["intel-speed-select", "-f", "json", "core-power", "disable"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        self.assertEqual(0, sst_comm_ret.returncode)
        pass

    def setUp(self):
        pass
        # Start each with  tf and cp enabled
        #self.disable_turbo_freq()


    def tearDown(self):
        pass

    def get_sst_corepower_assoc(self, core_idx):
        sst_comm=["intel-speed-select", "-f", "json", "-c", str(core_idx), "core-power", "get-assoc"]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        sst_comm_json=json.loads(sst_comm_ret.stderr)
        for key in sst_comm_json:
            sst_assoc=int(sst_comm_json[key]['get-assoc']['clos'])
        return sst_assoc

    def get_sst_corepower_config(clos_id):
        sst_comm=["intel-speed-select", "-f", "json", "core-power", "get-config", "-c", str(core_idx)]
        sst_comm_ret=subprocess.run(sst_comm, capture_output=True)
        sst_comm_json=json.loads(sst_comm_ret.stderr)
        for key in sst_comm_json:
            sst_assoc=int(sst_comm_json[key]['get-assoc']['clos'])
        return sst_assoc


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
            for begin_status in ["disable", "enable"]:
                sst_comm=["intel-speed-select", "-f", "json", "turbo-freq", begin_status, "-l", str(pkg)]
                sst_comm_ret=subprocess.run(sst_comm, capture_output=True)

                self.assertEqual(0, sst_comm_ret.returncode)

                turbofreq_status = geopmpy.pio.read_signal("SST::TURBO_ENABLE:ENABLE", "package", pkg)
                if begin_status == "disable":
                    assert_val = 0
                else:
                    assert_val = 1
                        
                self.assertEqual(assert_val, turbofreq_status)

            #Check write in each state
            for begin_status in ["disable", "enable"]:
                for toggle_status in range(2):
                    sst_comm=["intel-speed-select", "-f", "json", "turbo-freq", begin_status, "-l", str(pkg)]
                    sst_comm_ret=subprocess.run(sst_comm, capture_output=True)

                    self.assertEqual(0, sst_comm_ret.returncode)
                    geopmpy.pio.write_control("SST::TURBO_ENABLE", "package", pkg, toggle_status)

                    turbofreq_status = geopmpy.pio.read_signal("SST::TURBO_ENABLE:ENABLE", "package", pkg)
                    self.assertEqual(toggle_status, int(turbofreq_status))

    def test_cp_status(self):

        #TODO: Figure out why this is messing up all the tests
        #self.disable_turbo_freq()
        #self.disable_core_power()

        num_pkg = geopmpy.topo.num_domain('package')
        num_core = geopmpy.topo.num_domain('core')

        for pkg in range(num_pkg):
            core_idx=int(pkg*(num_core/num_pkg))

            # Test Read
            self.disable_turbo_freq()
            self.disable_core_power()
            geopm_cp_status = geopmpy.pio.read_signal("SST::COREPRIORITY_ENABLE:ENABLE","package", pkg)
            self.assertEqual(0.0, geopm_cp_status)

            self.enable_core_power()
            geopm_cp_status = geopmpy.pio.read_signal("SST::COREPRIORITY_ENABLE:ENABLE","package", pkg)
            self.assertEqual(1.0, geopm_cp_status)

            # Test Write
            state_list=[1.0, 0.0, 1.0]
            for state_idx in state_list:
                geopmpy.pio.write_control("SST::COREPRIORITY_ENABLE", "package", pkg, state_idx)
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
        pass

    def test_write_core_power_config(self):
        pass

    


if __name__ == '__main__':
    unittest.main()
