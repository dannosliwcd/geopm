/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "config.h"

#include "TRLFrequencyLimitDetector.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "geopm/Exception.hpp"
#include "geopm/Helper.hpp"
#include "geopm/PlatformIO.hpp"
#include "geopm/PlatformTopo.hpp"

namespace geopm
{
    TRLFrequencyLimitDetector::TRLFrequencyLimitDetector(
            PlatformIO &platform_io,
            const PlatformTopo &platform_topo)
        : m_package_count(platform_topo.num_domain(GEOPM_DOMAIN_PACKAGE))
        , m_core_count(platform_topo.num_domain(GEOPM_DOMAIN_CORE))
        , m_cpu_frequency_max(platform_io.read_signal("CPU_FREQUENCY_MAX", GEOPM_DOMAIN_BOARD, 0))
        , m_cpu_frequency_sticker(platform_io.read_signal("CPU_FREQUENCY_STICKER", GEOPM_DOMAIN_BOARD, 0))
        , m_cores_in_packages()
        // Initially assume we can reach single-core turbo limits
        , m_core_frequency_limits(m_core_count, { { m_core_count / m_package_count, m_cpu_frequency_max } } )
        , m_core_lp_frequencies(m_core_count, m_cpu_frequency_sticker)
    {
        for (unsigned int i = 0; i < m_package_count; ++i) {
            const auto nested_ctls = platform_topo.domain_nested(
                    GEOPM_DOMAIN_CORE, GEOPM_DOMAIN_PACKAGE, i);
            m_cores_in_packages.push_back(
                    std::vector<int>(nested_ctls.begin(), nested_ctls.end()));
        }
    }

    void TRLFrequencyLimitDetector::update_max_frequency_estimates(
            const std::vector<double> &observed_core_frequencies)
    {
        for (size_t package_idx = 0; package_idx < m_package_count; ++package_idx) {
            // SST-TF is not being considered. Assume any core can reach the
            // current max observed frequency across cores.
            const auto& cores_in_package = m_cores_in_packages[package_idx];
            const auto core_with_max_frequency = *std::max_element(
                    cores_in_package.begin(), cores_in_package.end(),
                    [&observed_core_frequencies] (int lhs, int rhs) {
                        return observed_core_frequencies[lhs] < observed_core_frequencies[rhs];
                    });
            const auto max_frequency = observed_core_frequencies[core_with_max_frequency];
            for (const auto core_idx : cores_in_package) {
                m_core_frequency_limits[core_idx] = {
                    { cores_in_package.size(), max_frequency }
                };
                m_core_lp_frequencies[core_idx] = m_cpu_frequency_sticker;
            }
        }
    }

    std::vector<std::pair<unsigned int, double> >
    TRLFrequencyLimitDetector::get_core_frequency_limits(unsigned int core_idx)
    {
#ifdef GEOPM_DEBUG
        if (core_idx >= m_core_frequency_limits.size()) {
            throw Exception("TRLFrequencyLimitDetector::" + std::string(__func__) +
                            "Encountered invalid core index: " +
                            std::to_string(core_idx), GEOPM_ERROR_INVALID,
                            __FILE__, __LINE__);
        }
#endif
        std::vector<std::map<unsigned int, double> > tradeoffs;

        return m_core_frequency_limits[core_idx];
    }

    double TRLFrequencyLimitDetector::get_core_low_priority_frequency(
            unsigned int core_idx)
    {
        return m_core_lp_frequencies[core_idx];
    }
}
