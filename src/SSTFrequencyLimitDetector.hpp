/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SSTFREQUENCYLIMITDETECTOR_HPP_INCLUDE
#define SSTFREQUENCYLIMITDETECTOR_HPP_INCLUDE

#include <FrequencyLimitDetector.hpp>

#include <utility>
#include <memory>
#include <vector>

namespace geopm
{
    class SSTFrequencyLimitDetector : public FrequencyLimitDetector {
        public:
            SSTFrequencyLimitDetector(
                    PlatformIO &platform_io,
                    const PlatformTopo &platform_topo);
            void update_max_frequency_estimates(
                    const std::vector<double> &observed_core_frequencies) override;
            std::vector<std::pair<unsigned int, double> > get_core_frequency_limits(
                    unsigned int core_idx) override;
            double get_core_low_priority_frequency(unsigned int core_idx) override;

        private:
            PlatformIO &m_platform_io;
            unsigned int m_package_count;
            size_t m_core_count;
            std::vector<int> m_clos_association_signals;
            std::vector<int> m_sst_tf_enable_signals;
            const double m_cpu_frequency_max;
            const double m_cpu_frequency_sticker;
            const double m_cpu_frequency_step;
            const double m_all_core_turbo_frequency;
            std::vector<unsigned int> m_bucket_hp_cores;
            const double m_low_priority_sse_frequency;
            const double m_low_priority_avx2_frequency;
            const double m_low_priority_avx512_frequency;

            // Vectors of license level frequency limits by bucket number
            std::vector<double> m_bucket_sse_frequency;
            std::vector<double> m_bucket_avx2_frequency;
            std::vector<double> m_bucket_avx512_frequency;
            std::vector<std::pair<unsigned int, double> > m_sse_hp_tradeoffs;
            std::vector<std::pair<unsigned int, double> > m_avx2_hp_tradeoffs;
            std::vector<std::pair<unsigned int, double> > m_avx512_hp_tradeoffs;
            std::vector<std::vector<int> > m_cores_in_packages;
            std::vector<std::vector<std::pair<unsigned int, double> > > m_core_frequency_limits;
            std::vector<double> m_core_lp_frequencies;
    };
}

#endif

