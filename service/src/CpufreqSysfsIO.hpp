/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CPUFREQSYSFSIO_HPP_INCLUDE
#define CPUFREQSYSFSIO_HPP_INCLUDE

#include <vector>
#include <map>

#include "SysfsIO.hpp"

namespace geopm
{
    /// @brief Class used to implement the CpufreqSysfsIOGroup
    class CpufreqSysfsIO: public SysfsIO
    {
        public:
            CpufreqSysfsIO();
            virtual ~CpufreqSysfsIO() = default;
            std::vector<std::string> signal_names(void) const override;
            std::vector<std::string> control_names(void) const override;
            std::string signal_path(const std::string &signal_name,
                                    int domain_type,
                                    int domain_idx) override;
            std::string control_path(const std::string &control_name,
                                     int domain_type,
                                     int domain_idx) const override;
            double signal_parse(const std::string &signal_name,
                                const std::string &content) const override;
            std::string control_gen(const std::string &control_name,
                                    double setting) const override;
            double signal_parse(int properties_id,
                                const std::string &content) const override;
            std::string control_gen(int properties_id,
                                    double setting) const override;
            std::string driver(void) const override;
            struct SysfsIO::properties_s properties(const std::string &name) const override;
            std::string properties_json(void) const override;
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
            const std::vector<SysfsIO::properties_s> m_properties;
            const std::map<std::string, SysfsIO::properties_s&> m_name_map;
    };
}

#endif
