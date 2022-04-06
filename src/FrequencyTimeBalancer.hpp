/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FREQUENCYTIMEBALANCER_HPP_INCLUDE
#define FREQUENCYTIMEBALANCER_HPP_INCLUDE

#include "FrequencyLimitDetector.hpp"

#include <memory>
#include <functional>
#include <vector>

namespace geopm
{
    /// Select frequency control settings that are expected to balance measured
    /// execution times. Assumes time impact of up to (frequency_old/frequency_new)
    /// percent. Workloads less frequency-sensitive than that should be able to
    /// go lower than the recommended frequencies. This is expected to converge
    /// toward those lower frequencies if it is repeatedly re-evaluated some
    /// time after applying the recommended frequency controls.
    class FrequencyTimeBalancer
    {
        public:
            virtual ~FrequencyTimeBalancer() = default;

            /// @brief Return the recommended frequency controls given observed
            ///        times while operating under a given set of previous
            ///        frequency controls.
            /// @param previous_times  Time spent in the region to be balanced,
            ///        measured by any domain.
            /// @param previous_control_frequencies  Frequency control last applied
            ///        over the region to be balanced, measured by the same
            ///        domain as \p previous_times.
            /// @param previous_achieved_frequencies  Average observed frequencies
            ///        over the region to be balanced, measured by the same
            ///        domain as \p previous_times.
            /// @param previous_max_frequencies  Max observed frequencies
            ///        over the region to be balanced, measured by the same
            ///        domain as \p previous_times.
            virtual std::vector<double> balance_frequencies_by_time(
                    const std::vector<double>& previous_times,
                    const std::vector<double>& previous_control_frequencies,
                    const std::vector<double>& previous_achieved_frequencies,
                    const std::vector<double>& previous_max_frequencies) = 0;

            virtual std::vector<double> get_target_times() const = 0;

            // @brief Return the throttling cutoff frequency for a given core.
            // @details The throttling cutoff frequency is a reference point for
            // balancing operations.
            // @param core the GEOPM topology index of the desired core.
            virtual double get_cutoff_frequency(unsigned int core) const = 0;

            /// @brief Allocate a FrequencyTimeBalancer instance.
            /// @param uncertainty_window_seconds  Width of a window of time
            ///        within which the balancer can consider two measured times
            ///        to be in balance with each other.
            /// @param sumdomain_group_count  Number of subdomains within the
            ///        domains to be balanced. E.g., to independently balance
            ///        each package within per-core times and frequencies,
            ///        this should be set to the number of packages on the node.
            /// @param do_ignore_domain_idx  A function that accepts an argument
            ///        indicating the domain index of a time to be balanced. The
            ///        function should return true if that domain index should
            ///        be excluded from balancing decisions. In that case, the
            ///        previous frequency for that index will be recommended as
            ///        the next frequency for that index.
            /// @param minimum_frequency  The lowest frequency control to allow
            ///        in rebalancing frequency control decisions.
            /// @param maximum_frequency  The highest frequency control to allow
            ///        in rebalancing frequency control decisions.
            /// @param frequency_limit_detector  An object that estimates
            ///        maximum achievable frequency of cores.
            static std::unique_ptr<FrequencyTimeBalancer> make_unique(
                    double uncertainty_window_seconds,
                    int subdomain_group_count,
                    std::function<bool(int)> do_ignore_domain_idx,
                    double minimum_frequency,
                    double maximum_frequency,
                    std::unique_ptr<FrequencyLimitDetector> frequency_limit_detector);

            /// \copydoc FrequencyTimeBalancer::make_unique
            static std::shared_ptr<FrequencyTimeBalancer> make_shared(
                    double uncertainty_window_seconds,
                    int subdomain_group_count,
                    std::function<bool(int)> do_ignore_domain_idx,
                    double minimum_frequency,
                    double maximum_frequency,
                    std::unique_ptr<FrequencyLimitDetector> frequency_limit_detector);
    };
}

#endif
