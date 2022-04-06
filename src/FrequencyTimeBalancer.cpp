/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.h"

#include "FrequencyTimeBalancer.hpp"

#include "geopm/Helper.hpp"
#include "geopm_debug.hpp"
#include "FrequencyLimitDetector.hpp"

#include <algorithm>
#include <numeric>
#include <cmath>

namespace geopm
{
    class FrequencyTimeBalancerImp : public FrequencyTimeBalancer
    {
        public:
            FrequencyTimeBalancerImp() = delete;
            FrequencyTimeBalancerImp(
                double uncertainty_window_seconds,
                int subdomain_group_count,
                std::function<bool(int)> do_ignore_domain_idx,
                double minimum_frequency,
                double maximum_frequency,
                std::unique_ptr<FrequencyLimitDetector> frequency_limit_detector);
            virtual ~FrequencyTimeBalancerImp() = default;

            std::vector<double> balance_frequencies_by_time(
                    const std::vector<double>& previous_times,
                    const std::vector<double>& previous_control_frequencies,
                    const std::vector<double>& previous_achieved_frequencies,
                    const std::vector<double>& previous_max_frequencies) override;

            std::vector<double> get_target_times() const override;

            double get_cutoff_frequency(unsigned int core) const override;

            static std::unique_ptr<FrequencyTimeBalancer> make_unique(
                    double uncertainty_window_seconds,
                    int subdomain_group_count,
                    std::function<bool(int)> do_ignore_domain_idx,
                    double minimum_frequency,
                    double maximum_frequency,
                    std::unique_ptr<FrequencyLimitDetector> frequency_limit_detector);
            static std::shared_ptr<FrequencyTimeBalancer> make_shared(
                    double uncertainty_window_seconds,
                    int subdomain_group_count,
                    std::function<bool(int)> do_ignore_domain_idx,
                    double minimum_frequency,
                    double maximum_frequency,
                    std::unique_ptr<FrequencyLimitDetector> frequency_limit_detector);
        private:
            double m_uncertainty_window_seconds;
            int m_subdomain_group_count;
            std::function<bool(int)> m_do_ignore_domain_idx;
            double m_minimum_frequency;
            double m_maximum_frequency;
            std::vector<double> m_target_times;
            std::vector<double> m_cutoff_frequencies;
            std::unique_ptr<FrequencyLimitDetector> m_frequency_limit_detector;
    };


    FrequencyTimeBalancerImp::FrequencyTimeBalancerImp(
            double uncertainty_window_seconds,
            int subdomain_group_count,
            std::function<bool(int)> do_ignore_domain_idx,
            double minimum_frequency,
            double maximum_frequency,
            std::unique_ptr<FrequencyLimitDetector> frequency_limit_detector)
        : m_uncertainty_window_seconds(uncertainty_window_seconds)
        , m_subdomain_group_count(subdomain_group_count)
        , m_do_ignore_domain_idx(do_ignore_domain_idx)
        , m_minimum_frequency(minimum_frequency)
        , m_maximum_frequency(maximum_frequency)
        , m_target_times(subdomain_group_count, NAN)
        , m_cutoff_frequencies(subdomain_group_count, NAN)
        , m_frequency_limit_detector(std::move(frequency_limit_detector))
    {
    }

    std::vector<double> FrequencyTimeBalancerImp::balance_frequencies_by_time(
                const std::vector<double>& previous_times,
                const std::vector<double>& previous_control_frequencies,
                const std::vector<double>& previous_achieved_frequencies,
                const std::vector<double>& previous_max_frequencies)
    {
        GEOPM_DEBUG_ASSERT(previous_times.size() == previous_control_frequencies.size(),
                           "FrequencyTimeBalancerImp::balance_frequencies_by_time(): "
                           "time vector must be the same size as the frequency vector.");

        m_frequency_limit_detector->update_max_frequency_estimates(previous_max_frequencies);

        const size_t domain_count_per_group =
            previous_control_frequencies.size() / m_subdomain_group_count;
        std::vector<double> desired_frequencies = previous_control_frequencies;

        std::vector<size_t> idx(previous_times.size());
        std::iota(idx.begin(), idx.end(), 0);

        // Perform an argsort, ordered by decreasing lagginess of a domain
        for (size_t group_idx = 0; group_idx < static_cast<size_t>(m_subdomain_group_count); ++group_idx) {
            std::sort(
                idx.begin() + group_idx * domain_count_per_group,
                idx.begin() + (group_idx + 1) * domain_count_per_group,
                [this, &previous_times, &previous_achieved_frequencies](size_t lhs, size_t rhs) {
                    // Place unrecorded times at the end of the sorted collection
                    if (std::isnan(previous_times[lhs]) || m_do_ignore_domain_idx(lhs)) {
                        return false;
                    }
                    if (std::isnan(previous_times[rhs]) || m_do_ignore_domain_idx(rhs)) {
                        return true;
                    }

                    // Sort by cycles in region of interest, rather than by
                    // time. Keep the most sensitive one at the front.
                    return previous_times[lhs] * previous_achieved_frequencies[lhs]
                         > previous_times[rhs] * previous_achieved_frequencies[rhs];
                });
        }

        for (size_t group_idx = 0;
             group_idx < static_cast<size_t>(m_subdomain_group_count); ++group_idx) {
            const auto group_idx_offset = group_idx * domain_count_per_group;
            const auto next_group_idx_offset = (group_idx + 1) * domain_count_per_group;

            if (std::find_if(previous_control_frequencies.begin() + group_idx_offset,
                             previous_control_frequencies.begin() + next_group_idx_offset,
                             [this](double frequency) {
                                 return frequency >= m_maximum_frequency;
                             }) == previous_control_frequencies.begin() + next_group_idx_offset) {
                // The previous iteration had no unlimited cores. Return to
                // baseline so we can make a better-informed decision next
                // iteration.
                std::fill(desired_frequencies.begin() + group_idx_offset,
                          desired_frequencies.begin() + next_group_idx_offset,
                          m_maximum_frequency);
                continue;
            }

            const auto lagger_time = previous_times[idx[group_idx_offset]];

            // Set the frequency of each index to match the last-observed
            // non-network time of the slowest recently-frequency-unlimited index.
            // We don't want to balance against frequency-limited indices in case
            // a previous frequency limit was set too low, which could set our
            // target performance too low.
            auto target_it = std::find_if(
                    idx.begin() + group_idx_offset,
                    idx.begin() + next_group_idx_offset,
                    [&previous_control_frequencies, this](size_t frequency_idx) {
                        return !m_do_ignore_domain_idx(frequency_idx) &&
                               previous_control_frequencies[frequency_idx] >= m_maximum_frequency;
                    });
            size_t reference_core_idx;
            if (target_it == idx.begin() + next_group_idx_offset) {
                reference_core_idx = idx[group_idx_offset];
            }
            else {
                reference_core_idx = *target_it;
            }

            // From previously-unthrottled cores, match the core with the most
            // time, scaled to the estimated time at its estimated
            // maximum-achievable frequency.
            double balance_target_time = previous_times[reference_core_idx];
            const auto high_priority_frequencies_by_core_count = m_frequency_limit_detector->get_core_frequency_limits(reference_core_idx);
            const auto reference_lp_frequency = get_cutoff_frequency(reference_core_idx);

            auto reference_core_hp_cutoff = m_minimum_frequency;

            // See if we can opt for an even lower desired time based on
            // prioritized frequency tradeoffs.
            for (const auto& hp_count_frequency : high_priority_frequencies_by_core_count) {
                const size_t hp_count = hp_count_frequency.first;
                const double hp_frequency = hp_count_frequency.second;

                // Estimate achieved vs achievable impacts
                const auto laggiest_high_priority_time =
                    lagger_time
                    * previous_achieved_frequencies[idx[group_idx_offset]]
                    / hp_frequency;
                if (hp_count < domain_count_per_group) {
                    const auto laggiest_lp_idx = idx[group_idx_offset + hp_count];
                    const auto laggiest_low_priority_time =
                        previous_times[laggiest_lp_idx]
                        * previous_achieved_frequencies[laggiest_lp_idx]
                        / reference_lp_frequency;
                    const auto predicted_long_pole = std::max(laggiest_low_priority_time, laggiest_high_priority_time);
                    if (predicted_long_pole < balance_target_time) {
                        balance_target_time = predicted_long_pole;
                        reference_core_hp_cutoff = reference_lp_frequency;
                    }
                }
                else {
                    // Does the all-high-priority projection improve our time estimate?
                    if (laggiest_high_priority_time < balance_target_time) {
                        balance_target_time = laggiest_high_priority_time;
                        reference_core_hp_cutoff = reference_lp_frequency;
                    }
                }
            }

            m_cutoff_frequencies[group_idx] = reference_core_hp_cutoff;

            m_target_times[group_idx] = balance_target_time;

            // Select the frequency that results in the target balanced time
            // for each frequency control domain index.
            double max_group_frequency = m_minimum_frequency;
            for (size_t group_ctl_idx = group_idx_offset; group_ctl_idx < next_group_idx_offset; ++group_ctl_idx) {
                const size_t ctl_idx = idx[group_ctl_idx];

                double desired_frequency =
                        previous_achieved_frequencies[ctl_idx]
                        * previous_times[ctl_idx]
                        / m_target_times[group_idx];
                bool is_lp = desired_frequency <= reference_core_hp_cutoff;
                desired_frequency += desired_frequency * m_uncertainty_window_seconds / balance_target_time;
                if (is_lp) {
                    desired_frequency = std::min(desired_frequency, reference_core_hp_cutoff);
                }

                if (!m_do_ignore_domain_idx(ctl_idx) && !std::isnan(desired_frequency)) {
                    desired_frequencies[ctl_idx] =
                        std::min(m_maximum_frequency, std::max(m_minimum_frequency, desired_frequency));
                    max_group_frequency = std::max(max_group_frequency, desired_frequencies[ctl_idx]);
                }
            }

            if (max_group_frequency < m_maximum_frequency) {
                auto frequency_scale = m_maximum_frequency / max_group_frequency;
                // Scale up only the cores that we want to be high priority.
                // Scale them far enough that the highest-frequency one is at
                // the maximum allowed frequency
                for (size_t ctl_idx = group_idx_offset; ctl_idx < next_group_idx_offset; ++ctl_idx) {
                    const auto ordered_ctl_idx = idx[ctl_idx];
                    if (!m_do_ignore_domain_idx(ordered_ctl_idx) && !std::isnan(desired_frequencies[ordered_ctl_idx])) {
                        if (desired_frequencies[ordered_ctl_idx] > reference_core_hp_cutoff) {
                            desired_frequencies[ordered_ctl_idx] =
                                std::min(m_maximum_frequency,
                                         std::max(m_minimum_frequency,
                                                  desired_frequencies[ordered_ctl_idx] * frequency_scale));
                        }
                        else {
                            desired_frequencies[ordered_ctl_idx] =
                                std::min(reference_core_hp_cutoff,
                                         std::max(m_minimum_frequency,
                                                  desired_frequencies[ordered_ctl_idx] * frequency_scale));
                        }
                    }
                }
            }
        }

        return desired_frequencies;
    }

    std::vector<double> FrequencyTimeBalancerImp::get_target_times() const
    {
        return m_target_times;
    }

    double FrequencyTimeBalancerImp::get_cutoff_frequency(unsigned int core) const
    {
        return m_frequency_limit_detector->get_core_low_priority_frequency(core);
    }

    std::unique_ptr<FrequencyTimeBalancer> FrequencyTimeBalancer::make_unique(
                double uncertainty_window_seconds,
                int subdomain_group_count,
                std::function<bool(int)> do_ignore_domain_idx,
                double minimum_frequency,
                double maximum_frequency,
                std::unique_ptr<FrequencyLimitDetector> frequency_limit_detector)
    {
        return geopm::make_unique<FrequencyTimeBalancerImp>(
                uncertainty_window_seconds,
                subdomain_group_count,
                do_ignore_domain_idx,
                minimum_frequency,
                maximum_frequency,
                std::move(frequency_limit_detector));
    }

    std::shared_ptr<FrequencyTimeBalancer> FrequencyTimeBalancer::make_shared(
                double uncertainty_window_seconds,
                int subdomain_group_count,
                std::function<bool(int)> do_ignore_domain_idx,
                double minimum_frequency,
                double maximum_frequency,
                std::unique_ptr<FrequencyLimitDetector> frequency_limit_detector)
    {
        return std::make_shared<FrequencyTimeBalancerImp>(
                uncertainty_window_seconds,
                subdomain_group_count,
                do_ignore_domain_idx,
                minimum_frequency,
                maximum_frequency,
                std::move(frequency_limit_detector));
    }
}
