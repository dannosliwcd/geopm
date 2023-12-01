/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CPUFREQSYSFSIO_HPP_INCLUDE
#define CPUFREQSYSFSIO_HPP_INCLUDE

#include <vector>
#include <map>

#include "SysfsDriver.hpp"

namespace geopm
{
    class IOGroup;

    /// @brief Class used to implement the CpufreqSysfsDriverGroup
    class CpufreqSysfsDriver: public SysfsDriver
    {
        public:
            CpufreqSysfsDriver();
            virtual ~CpufreqSysfsDriver() = default;
            int domain_type(const std::string &name) const override;
            std::string signal_path(const std::string &signal_name,
                                    int domain_idx) override;
            std::string control_path(const std::string &control_name,
                                     int domain_idx) const override;
            std::function<double(const std::string&)> signal_parse(const std::string &signal_name) const override;
            std::function<std::string(double)> control_gen(const std::string &control_name) const override;
            std::string driver(void) const override;
            std::map<std::string, SysfsDriver::properties_s> properties(void) const override;
            static std::string plugin_name(void);
            static std::unique_ptr<IOGroup> make_plugin(void);
        private:
            enum m_properties_index_e {
                M_BIOS_LIMIT,
                M_CUR_FREQ,
                M_MAX_FREQ,
                M_MIN_FREQ,
                M_TRANSITION_LATENCY,
                M_SCALING_CUR_FREQ,
                M_SCALING_MAX_FREQ,
                M_SCALING_MIN_FREQ,
                M_SCALING_SETSPEED,
                M_NUM_PROPERTIES
            };
            const std::map<std::string, SysfsDriver::properties_s> m_properties;
  };
}

#endif
