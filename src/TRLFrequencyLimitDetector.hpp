/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TRLFREQUENCYLIMITDETECTOR_HPP_INCLUDE
#define TRLFREQUENCYLIMITDETECTOR_HPP_INCLUDE

#include <FrequencyLimitDetector.hpp>

#include <utility>
#include <memory>
#include <vector>

namespace geopm
{
    // A frequency limit detector that depends on CPU package turbo ratio limits
    class TRLFrequencyLimitDetector : public FrequencyLimitDetector {
        public:
            TRLFrequencyLimitDetector(
                    PlatformIO &platform_io,
                    const PlatformTopo &);

            void update_max_frequency_estimates(
                    const std::vector<double> &observed_core_frequencies) override;
            std::vector<std::pair<unsigned int, double> > get_core_frequency_limits(
                    unsigned int core_idx) override;
            double get_core_low_priority_frequency(unsigned int core_idx) override;

        private:
            unsigned int m_package_count;
            size_t m_core_count;
            const double m_cpu_frequency_max;
            const double m_cpu_frequency_sticker;
            std::vector<std::vector<int> > m_cores_in_packages;
            std::vector<std::vector<std::pair<unsigned int, double> > > m_core_frequency_limits;
            std::vector<double> m_core_lp_frequencies;
    };
}

#endif

