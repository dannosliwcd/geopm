/*
 * Copyright (c) 2015 - 2021, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <cmath>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <sched.h>
#include <errno.h>

#include "DCGMIOGroup.hpp"
#include "DCGMDevicePool.hpp"
#include "geopm/PlatformTopo.hpp"
#include "geopm/Exception.hpp"
#include "geopm/Agg.hpp"
#include "geopm/Helper.hpp"

namespace geopm
{
    DCGMIOGroup::DCGMIOGroup()
        : DCGMIOGroup(platform_topo(), dcgm_device_pool())
    {
    }

    // Set up mapping between signal and control names and corresponding indices
    DCGMIOGroup::DCGMIOGroup(const PlatformTopo &platform_topo, DCGMDevicePool &device_pool)
        : m_platform_topo(platform_topo)
        , m_dcgm_device_pool(device_pool)
        , m_is_batch_read(false)
        , m_signal_available({{"DCGM::SM_ACTIVE", {
                                  "SM activity expressed as a ratio of cycles",
                                  {},
                                  [this](unsigned int domain_idx) -> double
                                  {
                                      return this->m_dcgm_device_pool.sample(
                                                   domain_idx, geopm::DCGMDevicePool::M_FIELD_ID_SM_ACTIVE);
                                  },
                                  Agg::average,
                                  string_format_double
                                  }},
                              {"DCGM::SM_OCCUPANCY", {
                                  "Warp residency expressed as a ratio of maximum warps per cycles",
                                  {},
                                  [this](unsigned int domain_idx) -> double
                                  {
                                      return this->m_dcgm_device_pool.sample(
                                                   domain_idx, geopm::DCGMDevicePool::M_FIELD_ID_SM_OCCUPANCY);
                                  },
                                  Agg::average,
                                  string_format_double
                                  }},
                              {"DCGM::DRAM_ACTIVE", {
                                  "DRAM Send & Receive expresed as a ratio of cycles",
                                  {},
                                  [this](unsigned int domain_idx) -> double
                                  {
                                      return this->m_dcgm_device_pool.sample(
                                                   domain_idx, geopm::DCGMDevicePool::M_FIELD_ID_DRAM_ACTIVE);
                                  },
                                  Agg::average,
                                  string_format_double
                                  }},
                             })
        , m_control_available({{"DCGM::FIELD_UPDATE_RATE", {
                                    "Rate at which field data is polled in seconds",
                                    {},
                                    Agg::expect_same,
                                    string_format_double
                                    }},
                               {"DCGM::MAX_STORAGE_TIME", {
                                    "Maximum time field data is stored in seconds",
                                    {},
                                    Agg::expect_same,
                                    string_format_double
                                    }},
                               {"DCGM::MAX_SAMPLES", {
                                    "Maximum number of samples.  0=no limit",
                                    {},
                                    Agg::expect_same,
                                    string_format_integer
                                    }}
                              })
    {
        // confirm all DCGM devices correspond to a board_accelerator
        if (m_dcgm_device_pool.num_device() != m_platform_topo.num_domain(GEOPM_DOMAIN_BOARD_ACCELERATOR)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": "
                            "DCGM enabled device count does not match BOARD_ACCELERATOR count",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        // populate signals for each domain
        for (auto &sv : m_signal_available) {
            std::vector<std::shared_ptr<signal_s> > result;
            for (int domain_idx = 0; domain_idx < m_platform_topo.num_domain(signal_domain_type(sv.first)); ++domain_idx) {
                std::shared_ptr<signal_s> sgnl = std::make_shared<signal_s>(signal_s{0, false});
                result.push_back(sgnl);
            }
            sv.second.m_signals = result;
        }
        register_signal_alias("GPU_COMPUTE_ACTIVITY", "DCGM::SM_ACTIVE");
        register_signal_alias("GPU_MEMORY_ACTIVITY", "DCGM::DRAM_ACTIVE");

        // populate controls for each domain
        for (auto &sv : m_control_available) {
            std::vector<std::shared_ptr<control_s> > result;
            for (int domain_idx = 0; domain_idx < m_platform_topo.num_domain(control_domain_type(sv.first)); ++domain_idx) {
                std::shared_ptr<control_s> ctrl = std::make_shared<control_s>(control_s{0, false});
                result.push_back(ctrl);
            }
            sv.second.m_controls = result;
        }
    }

    DCGMIOGroup::~DCGMIOGroup(void)
    {
    }

    // Extract the set of all signal names from the index map
    std::set<std::string> DCGMIOGroup::signal_names(void) const
    {
        std::set<std::string> result;
        for (const auto &sv : m_signal_available) {
            result.insert(sv.first);
        }
        return result;
    }

    // Extract the set of all control names from the index map
    std::set<std::string> DCGMIOGroup::control_names(void) const
    {
        std::set<std::string> result;
        for (const auto &sv : m_control_available) {
            result.insert(sv.first);
        }
        return result;
    }

    // Check signal name using index map
    bool DCGMIOGroup::is_valid_signal(const std::string &signal_name) const
    {
        return m_signal_available.find(signal_name) != m_signal_available.end();
    }

    // Check control name using index map
    bool DCGMIOGroup::is_valid_control(const std::string &control_name) const
    {
        return m_control_available.find(control_name) != m_control_available.end();
    }

    // Return domain for all valid signals
    int DCGMIOGroup::signal_domain_type(const std::string &signal_name) const
    {
        return is_valid_signal(signal_name) ? GEOPM_DOMAIN_BOARD_ACCELERATOR : GEOPM_DOMAIN_INVALID;
    }

    // Return domain for all valid controls
    int DCGMIOGroup::control_domain_type(const std::string &control_name) const
    {
        return is_valid_control(control_name) ? GEOPM_DOMAIN_BOARD : GEOPM_DOMAIN_INVALID;
    }

    // Mark the given signal to be read by read_batch()
    int DCGMIOGroup::push_signal(const std::string &signal_name, int domain_type, int domain_idx)
    {
        if (!is_valid_signal(signal_name)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": signal_name " + signal_name +
                            " not valid for DCGMIOGroup.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if (domain_type != signal_domain_type(signal_name)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": " + signal_name + ": domain_type must be " +
                            std::to_string(signal_domain_type(signal_name)),
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if (domain_idx < 0 || domain_idx >= m_platform_topo.num_domain(signal_domain_type(signal_name))) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": domain_idx out of range.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if (m_is_batch_read) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": cannot push signal after call to read_batch().",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        int result = -1;
        bool is_found = false;
        std::shared_ptr<signal_s> signal = m_signal_available.at(signal_name).m_signals.at(domain_idx);

        // Check if signal was already pushed
        for (size_t ii = 0; !is_found && ii < m_signal_pushed.size(); ++ii) {
            // same location means this signal or its alias was already pushed
            if (m_signal_pushed[ii].get() == signal.get()) {
                result = ii;
                is_found = true;
            }
        }
        if (!is_found) {
            // If not pushed, add to pushed signals and configure for batch reads
            result = m_signal_pushed.size();
            signal->m_do_read = true;
            m_signal_pushed.push_back(signal);
        }

        return result;
    }

    // Mark the given control to be written by write_batch()
    int DCGMIOGroup::push_control(const std::string &control_name, int domain_type, int domain_idx)
    {
        if (!is_valid_control(control_name)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": control_name " + control_name +
                            " not valid for DCGMIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if (domain_type != control_domain_type(control_name)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": " + control_name + ": domain_type must be " +
                            std::to_string(control_domain_type(control_name)),
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if (domain_idx < 0 || domain_idx >= m_platform_topo.num_domain(domain_type)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": domain_idx out of range.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        int result = -1;
        bool is_found = false;
        std::shared_ptr<control_s> control = m_control_available.at(control_name).m_controls.at(domain_idx);

        // Check if control was already pushed
        for (size_t ii = 0; !is_found && ii < m_control_pushed.size(); ++ii) {
            // same location means this control or its alias was already pushed
            if (m_control_pushed[ii] == control) {
                result = ii;
                is_found = true;
            }
        }
        if (!is_found) {
            // If not pushed, add to pushed control
            result = m_control_pushed.size();
            m_control_pushed.push_back(control);
        }

        return result;
    }

    // Parse and update saved values for signals
    void DCGMIOGroup::read_batch(void)
    {
        m_is_batch_read = true;
        if(!m_signal_pushed.empty()) {
            // NOTE: Doing this requires all signals to operate at the
            //       GEOPM_BOARD_ACCELERATOR domain, but it means
            //       dcgmGetLatestValuesForFields only has to be called
            //       once per GEOPM_BOARD_ACCELERATOR domain.
            for (int domain_idx = 0; domain_idx < m_platform_topo.num_domain(
                 GEOPM_DOMAIN_BOARD_ACCELERATOR); ++domain_idx) {

                m_dcgm_device_pool.update(domain_idx);

                for (auto &sv : m_signal_available) {
                    if (sv.second.m_signals.at(domain_idx)->m_do_read) {
                        sv.second.m_signals.at(domain_idx)->m_value =
                            sv.second.m_devpool_func(domain_idx);
                    }
                }
            }
        }
    }

    // Write all controls that have been pushed and adjusted
    void DCGMIOGroup::write_batch(void)
    {
        for (auto &sv : m_control_available) {
            for (unsigned int domain_idx = 0; domain_idx < sv.second.m_controls.size(); ++domain_idx) {
                if (sv.second.m_controls.at(domain_idx)->m_is_adjusted) {
                    write_control(sv.first, control_domain_type(sv.first), domain_idx,
                                  sv.second.m_controls.at(domain_idx)->m_setting);
                }
            }
        }
    }

    // Return the latest value read by read_batch()
    double DCGMIOGroup::sample(int batch_idx)
    {
        // Do conversion of signal values stored in read batch
        if (batch_idx < 0 || batch_idx >= (int)m_signal_pushed.size()) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": batch_idx " +std::to_string(batch_idx)+ " out of range",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if (!m_is_batch_read) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": signal has not been read.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        return m_signal_pushed[batch_idx]->m_value;
    }

    // Save a setting to be written by a future write_batch()
    void DCGMIOGroup::adjust(int batch_idx, double setting)
    {
        if (batch_idx < 0 || (unsigned)batch_idx >= m_control_pushed.size()) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + "(): batch_idx " +std::to_string(batch_idx)+ " out of range",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        m_control_pushed.at(batch_idx)->m_setting = setting;
        m_control_pushed.at(batch_idx)->m_is_adjusted = true;
    }

    // Read the value of a signal immediately, bypassing read_batch().  Should not modify m_signal_value
    double DCGMIOGroup::read_signal(const std::string &signal_name, int domain_type, int domain_idx)
    {
        if (!is_valid_signal(signal_name)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": " + signal_name +
                            " not valid for DCGMIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if (domain_type != signal_domain_type(signal_name)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": " + signal_name + ": domain_type must be " +
                            std::to_string(signal_domain_type(signal_name)),
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if (domain_idx < 0 || domain_idx >= m_platform_topo.num_domain(signal_domain_type(signal_name))) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": domain_idx out of range.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        double result = NAN;
        m_dcgm_device_pool.update(domain_idx);

        auto it = m_signal_available.find(signal_name);
        if (it != m_signal_available.end()) {
            result = it->second.m_devpool_func(domain_idx);
        }
        else {
    #ifdef GEOPM_DEBUG
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": Handling not defined for " +
                            signal_name, GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    #endif
        }
        return result;
    }

    // Write to the control immediately, bypassing write_batch()
    void DCGMIOGroup::write_control(const std::string &control_name, int domain_type, int domain_idx, double setting)
    {
        if (!is_valid_control(control_name)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": " + control_name +
                            " not valid for DCGMIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if (domain_type != control_domain_type(control_name)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": " + control_name + ": domain_type must be " +
                            std::to_string(control_domain_type(control_name)),
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if (domain_idx < 0 || domain_idx >= m_platform_topo.num_domain(GEOPM_DOMAIN_BOARD)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": domain_idx out of range.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        if (control_name == "DCGM::FIELD_UPDATE_RATE") {
            m_dcgm_device_pool.update_rate(setting*1e6);
        }
        else if (control_name == "DCGM::MAX_STORAGE_TIME") {
            m_dcgm_device_pool.max_storage_time(setting);
        }
        else if (control_name == "DCGM::MAX_SAMPLES") {
            m_dcgm_device_pool.max_samples(setting);
        }
        else {
    #ifdef GEOPM_DEBUG
                throw Exception("DCGMIOGroup::" + std::string(__func__) + "Handling not defined for "
                                + control_name, GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    #endif
        }
    }

    // Implemented to allow an IOGroup to save platform settings before starting
    // to adjust them
    void DCGMIOGroup::save_control(void)
    {
    }

    // Implemented to allow an IOGroup to restore previously saved
    // platform settings
    void DCGMIOGroup::restore_control(void)
    {
        // The 1 second update rate used here is the default sample rate discussed
        // in the DCGM user guide.
        m_dcgm_device_pool.update_rate(1e6); //1 second

        // The max samples restore value is based upon the defaults used in DCGM
        // sdk example code for field value access provided as part of the DCGM install
        m_dcgm_device_pool.max_samples(3600); //3600 samples

        // The max storage time restore value is based upon the defaults used in DCGM
        // sdk example code for field value access provided as part of the DCGM install
        m_dcgm_device_pool.max_storage_time(3600); //3600 seconds
    }

    void DCGMIOGroup::save_control(const std::string &save_path)
    {
    }

    void DCGMIOGroup::restore_control(const std::string &save_path)
    {
        restore_control();
    }

    // Hint to Agent about how to aggregate signals from this IOGroup
    std::function<double(const std::vector<double> &)> DCGMIOGroup::agg_function(const std::string &signal_name) const
    {
        auto it = m_signal_available.find(signal_name);
        if (it == m_signal_available.end()) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": " + signal_name +
                            "not valid for DCGMIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        return it->second.m_agg_function;
    }

    // Specifies how to print signals from this IOGroup
    std::function<std::string(double)> DCGMIOGroup::format_function(const std::string &signal_name) const
    {
        auto it = m_signal_available.find(signal_name);
        if (it == m_signal_available.end()) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": " + signal_name +
                            "not valid for DCGMIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        return it->second.m_format_function;
    }

    // A user-friendly description of each signal
    std::string DCGMIOGroup::signal_description(const std::string &signal_name) const
    {
        if (!is_valid_signal(signal_name)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": signal_name " + signal_name +
                            " not valid for DCGMIOGroup.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        return m_signal_available.at(signal_name).m_description;
    }

    // A user-friendly description of each control
    std::string DCGMIOGroup::control_description(const std::string &control_name) const
    {
        if (!is_valid_control(control_name)) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": " + control_name +
                            "not valid for DCGMIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        return m_control_available.at(control_name).m_description;
    }

    int DCGMIOGroup::signal_behavior(const std::string &signal_name) const
    {
        return IOGroup::M_SIGNAL_BEHAVIOR_VARIABLE;
    }

    std::string DCGMIOGroup::name(void) const
    {
        return plugin_name();
    }

    // Name used for registration with the IOGroup factory
    std::string DCGMIOGroup::plugin_name(void)
    {
        return "DCGM";
    }

    // Function used by the factory to create objects of this type
    std::unique_ptr<IOGroup> DCGMIOGroup::make_plugin(void)
    {
        return geopm::make_unique<DCGMIOGroup>();
    }

    void DCGMIOGroup::register_signal_alias(const std::string &alias_name,
                                            const std::string &signal_name)
    {
        if (m_signal_available.find(alias_name) != m_signal_available.end()) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": signal_name " + alias_name +
                            " was previously registered.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        auto it = m_signal_available.find(signal_name);
        if (it == m_signal_available.end()) {
            // skip adding an alias if underlying signal is not found
            return;
        }
        // copy signal info but append to description
        m_signal_available[alias_name] = it->second;
        m_signal_available[alias_name].m_description =
            m_signal_available[signal_name].m_description + '\n' + "    alias_for: " + signal_name;
    }

    void DCGMIOGroup::register_control_alias(const std::string &alias_name,
                                           const std::string &control_name)
    {
        if (m_control_available.find(alias_name) != m_control_available.end()) {
            throw Exception("DCGMIOGroup::" + std::string(__func__) + ": contro1_name " + alias_name +
                            " was previously registered.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        auto it = m_control_available.find(control_name);
        if (it == m_control_available.end()) {
            // skip adding an alias if underlying control is not found
            return;
        }
        // copy control info but append to description
        m_control_available[alias_name] = it->second;
        m_control_available[alias_name].m_description =
        m_control_available[control_name].m_description + '\n' + "    alias_for: " + control_name;
    }
}
