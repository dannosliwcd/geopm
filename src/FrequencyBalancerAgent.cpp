/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FrequencyBalancerAgent.hpp"

#include <exception>
#include <iterator>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <thread>
#include <iostream>
#include <numeric>
#include <utility>
#include "geopm/Exception.hpp"
#include "geopm/Helper.hpp"
#include "geopm/PlatformIO.hpp"
#include "geopm/PlatformTopo.hpp"
#include "PowerGovernor.hpp"
#include "FrequencyGovernor.hpp"
#include "FrequencyTimeBalancer.hpp"
#include "FrequencyLimitDetector.hpp"
#include "SSTClosGovernor.hpp"
#include "geopm_debug.hpp"
#include "geopm_hash.h"
#include "geopm_hint.h"
#include "geopm_time.h"
#include "geopm_topo.h"
#include "config.h"

// Minimum number of sampling wait periods before applying new epoch controls.
#define MINIMUM_WAIT_PERIODS_FOR_NEW_EPOCH_CONTROL 5

// Minumum number of epochs to wait before applying new epoch controls.
#define MINIMUM_EPOCHS_FOR_NEW_EPOCH_CONTROL 3

// Number of back-to-back network hints to treat as "in a network region."
// Lower numbers respond more quickly, but risk throttling regions that
// happen to land next to a short-running newtork region.
// -- Arbitrarily set to 3 because that produces acceptable behavior so far.
#define NETWORK_HINT_MINIMUM_SAMPLE_LENGTH 3

// Number of back-to-back non-network hints to treat as not "in a network region."
#define NON_NETWORK_HINT_MINIMUM_SAMPLE_LENGTH 1

namespace geopm
{
    FrequencyBalancerAgent::FrequencyBalancerAgent()
        : FrequencyBalancerAgent(platform_io(), platform_topo(),
                                PowerGovernor::make_shared(),
                                FrequencyGovernor::make_shared(),
                                (SSTClosGovernor::is_supported(platform_io())
                                 ? SSTClosGovernor::make_shared()
                                 : nullptr),
                                nullptr)
    {
    }

    static bool is_all_nan(const std::vector<double> &vec)
    {
        return std::all_of(vec.begin(), vec.end(),
                           [](double x) -> bool { return std::isnan(x); });
    }

    FrequencyBalancerAgent::FrequencyBalancerAgent(PlatformIO &plat_io,
                                                 const PlatformTopo &topo,
                                                 std::shared_ptr<PowerGovernor> power_gov,
                                                 std::shared_ptr<FrequencyGovernor> frequency_gov,
                                                 std::shared_ptr<SSTClosGovernor> sst_gov,
                                                 std::shared_ptr<FrequencyTimeBalancer> frequency_time_balancer)
        : M_WAIT_SEC(0.005)
        , M_UNCERTAINTY_WINDOW_SECONDS(1.0 * M_WAIT_SEC)
        , m_platform_io(plat_io)
        , m_platform_topo(topo)
        , m_wait_time(time_zero())
        , m_update_time(time_zero())
        , m_num_children(0)
        , m_is_policy_updated(false)
        , m_do_write_batch(false)
        , m_is_adjust_initialized(false)
        , m_is_real_policy(false)
        , m_package_count(m_platform_topo.num_domain(GEOPM_DOMAIN_PACKAGE))
        , m_policy_power_package_limit_total(NAN)
        , m_policy_use_frequency_limits(true)
        , m_use_sst_tf(false)
        , m_min_power_setting(m_platform_io.read_signal("CPU_POWER_MIN_AVAIL",
                                                        GEOPM_DOMAIN_BOARD, 0))
        , m_max_power_setting(m_platform_io.read_signal("CPU_POWER_MAX_AVAIL",
                                                        GEOPM_DOMAIN_BOARD, 0))
        , m_tdp_power_setting(m_platform_io.read_signal("CPU_POWER_LIMIT_DEFAULT",
                                                        GEOPM_DOMAIN_BOARD, 0))
        , m_frequency_min(
              m_platform_io.read_signal("CPU_FREQUENCY_MIN_AVAIL", GEOPM_DOMAIN_BOARD, 0))
        , m_frequency_sticker(m_platform_io.read_signal("CPU_FREQUENCY_STICKER",
                                                        GEOPM_DOMAIN_BOARD, 0))
        , m_frequency_max(
              m_platform_io.read_signal("CPU_FREQUENCY_MAX_AVAIL", GEOPM_DOMAIN_BOARD, 0))
        , m_frequency_step(
              m_platform_io.read_signal("CPU_FREQUENCY_STEP", GEOPM_DOMAIN_BOARD, 0))
        , m_power_gov(power_gov)
        , m_freq_governor(frequency_gov)
        , m_sst_clos_governor(sst_gov)
        , m_frequency_ctl_domain_type(m_freq_governor->frequency_domain_type())
        , m_frequency_control_domain_count(m_platform_topo.num_domain(m_frequency_ctl_domain_type))
        , m_network_hint_sample_length(m_frequency_control_domain_count, 0)
        , m_non_network_hint_sample_length(m_frequency_control_domain_count, 0)
        , m_last_hp_count(2, 0)
        , m_handle_new_epoch(false)
        , m_epoch_wait_count(MINIMUM_EPOCHS_FOR_NEW_EPOCH_CONTROL)
        , m_frequency_time_balancer(frequency_time_balancer)
    {
        if (!m_frequency_time_balancer) {
            m_frequency_time_balancer = FrequencyTimeBalancer::make_unique(
                    M_UNCERTAINTY_WINDOW_SECONDS,
                    m_package_count,
                    [this](int core_idx) {
                        return std::isnan(m_last_hash[core_idx])
                               || m_last_hash[core_idx] == GEOPM_REGION_HASH_INVALID;
                    }, m_frequency_min, m_frequency_max,
                    FrequencyLimitDetector::make_unique(m_platform_io, m_platform_topo));
        }

        if (m_sst_clos_governor) {
            m_frequency_ctl_domain_type = m_sst_clos_governor->clos_domain_type();
            m_frequency_control_domain_count = m_platform_topo.num_domain(m_frequency_ctl_domain_type);
            m_network_hint_sample_length.resize(m_frequency_control_domain_count, 0);
            m_non_network_hint_sample_length.resize(m_frequency_control_domain_count, 0);
        }
    }

    std::string FrequencyBalancerAgent::plugin_name(void)
    {
        return "frequency_balancer";
    }

    std::unique_ptr<Agent> FrequencyBalancerAgent::make_plugin(void)
    {
        return geopm::make_unique<FrequencyBalancerAgent>();
    }

    void FrequencyBalancerAgent::init(int level, const std::vector<int> &fan_in,
                                     bool is_level_root)
    {
        static_cast<void>(is_level_root);
        if (level == 0) {
            m_num_children = 0;
            init_platform_io();
        }
        else {
            m_num_children = fan_in[level - 1];
        }

        m_platform_io.write_control("CPU_FREQUENCY_MAX_CONTROL", GEOPM_DOMAIN_BOARD, 0, m_frequency_max);
    }

    void FrequencyBalancerAgent::validate_policy(std::vector<double> &policy) const
    {
        GEOPM_DEBUG_ASSERT(policy.size() == M_NUM_POLICY,
                           "FrequencyBalancerAgent::validate_policy(): "
                           "policy vector not correctly sized.");

        if (std::isnan(policy[M_POLICY_POWER_PACKAGE_LIMIT_TOTAL])) {
            policy[M_POLICY_POWER_PACKAGE_LIMIT_TOTAL] = m_tdp_power_setting;
        }
        if (policy[M_POLICY_POWER_PACKAGE_LIMIT_TOTAL] < m_min_power_setting) {
            policy[M_POLICY_POWER_PACKAGE_LIMIT_TOTAL] = m_min_power_setting;
        }
        else if (policy[M_POLICY_POWER_PACKAGE_LIMIT_TOTAL] > m_max_power_setting) {
            policy[M_POLICY_POWER_PACKAGE_LIMIT_TOTAL] = m_max_power_setting;
        }

        if (std::isnan(policy[M_POLICY_USE_FREQUENCY_LIMITS])) {
            policy[M_POLICY_USE_FREQUENCY_LIMITS] = 1;
        }

        if (std::isnan(policy[M_POLICY_USE_SST_TF])) {
            policy[M_POLICY_USE_SST_TF] = 1;
        }

        if (policy[M_POLICY_USE_FREQUENCY_LIMITS] == 0 && policy[M_POLICY_USE_SST_TF] == 0) {
            throw Exception("FrequencyBalancerAgent::validate_policy(): must allow at "
                            "least one of frequency limits or SST-TF.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
    }

    void FrequencyBalancerAgent::update_policy(const std::vector<double> &policy)
    {
        if (is_all_nan(policy) && !m_is_real_policy) {
            // All-NAN policy is ignored until first real policy is received
            m_is_policy_updated = false;
            return;
        }
        m_is_real_policy = true;
    }

    void FrequencyBalancerAgent::split_policy(const std::vector<double> &in_policy,
                                             std::vector<std::vector<double> > &out_policy)
    {
#ifdef GEOPM_DEBUG
        if (out_policy.size() != (size_t)m_num_children) {
            throw Exception("FrequencyBalancerAgent::" + std::string(__func__) +
                                "(): out_policy vector not correctly sized.",
                            GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
        }
        for (auto &child_policy : out_policy) {
            if (child_policy.size() != M_NUM_POLICY) {
                throw Exception("FrequencyBalancerAgent::" + std::string(__func__) +
                                    "(): child_policy vector not correctly sized.",
                                GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
            }
        }
#endif
        update_policy(in_policy);

        if (m_is_policy_updated) {
            std::fill(out_policy.begin(), out_policy.end(), in_policy);
        }
    }

    bool FrequencyBalancerAgent::do_send_policy(void) const
    {
        return m_is_policy_updated;
    }

    void FrequencyBalancerAgent::aggregate_sample(const std::vector<std::vector<double> > &in_sample,
                                                 std::vector<double> &out_sample)
    {
        static_cast<void>(in_sample);
        static_cast<void>(out_sample);
    }

    bool FrequencyBalancerAgent::do_send_sample(void) const
    {
        return false;
    }

    void FrequencyBalancerAgent::initialize_policies(const std::vector<double> &in_policy)
    {
        double power_budget_in = in_policy[M_POLICY_POWER_PACKAGE_LIMIT_TOTAL];
        m_policy_power_package_limit_total = power_budget_in;
        double adjusted_power;
        m_power_gov->adjust_platform(power_budget_in, adjusted_power);
        m_do_write_batch = true;

        m_policy_use_frequency_limits = in_policy[M_POLICY_USE_FREQUENCY_LIMITS] != 0;
        bool sst_tf_is_supported = m_sst_clos_governor && SSTClosGovernor::is_supported(m_platform_io);
        m_use_sst_tf = in_policy[M_POLICY_USE_SST_TF] != 0 && sst_tf_is_supported;

        m_freq_governor->set_frequency_bounds(m_frequency_min, m_frequency_max);

        // Initialize to max frequency limit and max CLOS
        m_freq_governor->adjust_platform(m_last_ctl_frequency);
        if (sst_tf_is_supported) {
            if (m_use_sst_tf) {
                m_sst_clos_governor->adjust_platform(m_last_ctl_clos);
                m_sst_clos_governor->enable_sst_turbo_prioritization();
            } else {
                m_sst_clos_governor->disable_sst_turbo_prioritization();
            }
        }
    }

    void FrequencyBalancerAgent::adjust_platform(const std::vector<double> &in_policy)
    {
        update_policy(in_policy);

        m_do_write_batch = false;
        if (!m_is_adjust_initialized) {
            initialize_policies(in_policy);
            m_is_adjust_initialized = true;
            return;
        }

        std::vector<double> frequency_by_core = m_last_ctl_frequency;
        std::vector<double> clos_by_core = m_last_ctl_clos;

        if (m_handle_new_epoch) {
            m_handle_new_epoch = false;
            frequency_by_core = m_frequency_time_balancer->balance_frequencies_by_time(
                    m_last_epoch_non_network_time_diff,
                    m_last_ctl_frequency,
                    m_last_epoch_frequency,
                    m_last_epoch_max_frequency);
            for (size_t ctl_idx = 0; ctl_idx < frequency_by_core.size(); ++ctl_idx) {
                if (m_last_ctl_frequency[ctl_idx] > frequency_by_core[ctl_idx]) {
                    // Going down. Round up toward previous.
                    frequency_by_core[ctl_idx] = std::ceil(frequency_by_core[ctl_idx] / m_frequency_step) * m_frequency_step;
                }
                else {
                    // Going up. Round down toward previous.
                    frequency_by_core[ctl_idx] = std::floor(frequency_by_core[ctl_idx] / m_frequency_step) * m_frequency_step;
                }
            }
            geopm_time(&m_update_time);
            m_last_ctl_frequency = frequency_by_core;
        }

        // Apply immediate controls for workloads that change rapidly within
        // epochs.
        auto immediate_ctl_frequency = frequency_by_core;
        size_t num_frequency_control_domain_per_package = m_frequency_control_domain_count / m_package_count;
        for (size_t package_idx = 0;
             package_idx < static_cast<size_t>(m_package_count); ++package_idx) {
            int hp_not_waiting_count = 0;
            for (size_t local_idx = 0; local_idx < num_frequency_control_domain_per_package; ++local_idx) {
                const size_t ctl_idx =
                    package_idx * num_frequency_control_domain_per_package + local_idx;

                if (std::isnan(m_last_hash[ctl_idx]) || m_last_hash[ctl_idx] == GEOPM_REGION_HASH_INVALID) {
                    // Non-application regions get the expected low-priority
                    // frequency so we can focus our turbo budget on
                    // application regions.
                    immediate_ctl_frequency[ctl_idx] = m_frequency_time_balancer->get_cutoff_frequency(ctl_idx);
                }
                else if(m_network_hint_sample_length[ctl_idx] >= NETWORK_HINT_MINIMUM_SAMPLE_LENGTH) {
                    // Don't assume that (last hint)==NETWORK alone implies
                    // that we are in a network region because it may be a
                    // short-lasting region that we just happened to sample.
                    immediate_ctl_frequency[ctl_idx] = m_frequency_time_balancer->get_cutoff_frequency(ctl_idx);
                } else if (immediate_ctl_frequency[ctl_idx] >= m_frequency_max) {
                    // This is a HP core that is not in a networking region
                    hp_not_waiting_count += 1;
                }
            }

            if (hp_not_waiting_count == 0) {
                // All HP cores are waiting... Move the all non-waiting cores to HP
                for (size_t local_idx = 0; local_idx < num_frequency_control_domain_per_package; ++local_idx) {
                    const size_t ctl_idx =
                        package_idx * num_frequency_control_domain_per_package + local_idx;
                    if(m_non_network_hint_sample_length[ctl_idx] >= NON_NETWORK_HINT_MINIMUM_SAMPLE_LENGTH) {
                        immediate_ctl_frequency[ctl_idx] = m_frequency_max;
                    }
                }
            }
        }

        if (m_use_sst_tf) {
            // Assign to low priority CLOS if we expect that the core's
            // workload properties allow it to achieve our requested frequency
            // while in the low priority CLOS configuration. Otherwise, set it
            // to high priority CLOS.
            // The reason for selecting priority this way is that we are taking
            // a conservative approach to risks of over-throttling a core vs
            // missed opportunities from underthrottling cores.
            for (size_t ctl_idx = 0; ctl_idx < frequency_by_core.size(); ++ctl_idx) {
                clos_by_core[ctl_idx] =
                    (immediate_ctl_frequency[ctl_idx] > m_frequency_time_balancer->get_cutoff_frequency(ctl_idx))
                    ? SSTClosGovernor::HIGH_PRIORITY
                    : SSTClosGovernor::LOW_PRIORITY;
            }
            m_sst_clos_governor->adjust_platform(clos_by_core);
        }
        if (m_policy_use_frequency_limits) {
            m_freq_governor->adjust_platform(immediate_ctl_frequency);
        }
    }


    bool FrequencyBalancerAgent::do_write_batch(void) const
    {
        bool do_write = m_do_write_batch || m_freq_governor->do_write_batch();
        if (m_use_sst_tf) {
            do_write |= m_sst_clos_governor->do_write_batch();
        }
        return do_write;
    }

    void FrequencyBalancerAgent::sample_platform(std::vector<double> &out_sample)
    {
        static_cast<void>(out_sample);

        for (size_t ctl_idx = 0;
             ctl_idx < (size_t)m_frequency_control_domain_count; ++ctl_idx) {
            m_last_hash[ctl_idx] = m_platform_io.sample(m_hash_signal_idx[ctl_idx]);
            auto last_hint = m_platform_io.sample(m_hint_signal_idx[ctl_idx]);
            if (last_hint == GEOPM_REGION_HINT_NETWORK) {
                m_network_hint_sample_length[ctl_idx] += 1;
                m_non_network_hint_sample_length[ctl_idx] = 0;
            }
            else {
                m_network_hint_sample_length[ctl_idx] = 0;
                m_non_network_hint_sample_length[ctl_idx] += 1;
            }

            const auto prev_sample_acnt = m_last_sample_acnt[ctl_idx];
            const auto prev_sample_mcnt = m_last_sample_mcnt[ctl_idx];
            m_last_sample_acnt[ctl_idx] = m_platform_io.sample(m_acnt_signal_idx[ctl_idx]);
            m_last_sample_mcnt[ctl_idx] = m_platform_io.sample(m_mcnt_signal_idx[ctl_idx]);
            const auto last_sample_frequency =
                (m_last_sample_acnt[ctl_idx] - prev_sample_acnt) /
                (m_last_sample_mcnt[ctl_idx] - prev_sample_mcnt) * m_frequency_sticker;
            m_current_epoch_max_frequency[ctl_idx] = std::max(m_current_epoch_max_frequency[ctl_idx], last_sample_frequency);
        }

        double epoch_count = m_platform_io.sample(m_epoch_signal_idx);
        const auto counted_epochs = epoch_count - m_last_epoch_count;
        if (!std::isnan(epoch_count) && !std::isnan(m_last_epoch_count) &&
            counted_epochs >= m_epoch_wait_count) {
            double new_epoch_time = m_platform_io.read_signal(
                "TIME", GEOPM_DOMAIN_BOARD, 0);
            auto last_epoch_time_diff = new_epoch_time - m_last_epoch_time;
            if (last_epoch_time_diff < MINIMUM_WAIT_PERIODS_FOR_NEW_EPOCH_CONTROL * M_WAIT_SEC) {
                // Wait for some number of epochs to pass before calculating
                // time per epoch. The wait period should end at the boundary of
                // an epoch, but the minimum wait length depends on our sample
                // rate. We are trying to reduce the impact of aliasing on the
                // TIME_HINT_NETWORK signal.
                m_epoch_wait_count += 1;
            }
            else {
                // We encountered a new epoch and we are ready to evaluate the
                // signal differences over the previous group of epochs.
                for (size_t ctl_idx = 0;
                     ctl_idx < (size_t)m_frequency_control_domain_count; ++ctl_idx) {
                    double new_epoch_network_time =
                        m_platform_io.sample(m_time_hint_network_idx[ctl_idx]);
                    auto last_epoch_network_time_diff =
                        new_epoch_network_time - m_last_epoch_network_time[ctl_idx];
                    m_last_epoch_non_network_time_diff[ctl_idx] =
                        std::max(0.0, last_epoch_time_diff - last_epoch_network_time_diff) /
                        counted_epochs;
                    m_last_epoch_network_time[ctl_idx] = new_epoch_network_time;

                    const auto prev_epoch_acnt = m_last_epoch_acnt[ctl_idx];
                    const auto prev_epoch_mcnt = m_last_epoch_mcnt[ctl_idx];
                    m_last_epoch_acnt[ctl_idx] =
                        m_platform_io.sample(m_acnt_signal_idx[ctl_idx]);
                    m_last_epoch_mcnt[ctl_idx] =
                        m_platform_io.sample(m_mcnt_signal_idx[ctl_idx]);
                    m_last_epoch_frequency[ctl_idx] =
                        (m_last_epoch_acnt[ctl_idx] - prev_epoch_acnt) /
                        (m_last_epoch_mcnt[ctl_idx] - prev_epoch_mcnt) * m_frequency_sticker;
                }
                m_last_epoch_max_frequency.swap(m_current_epoch_max_frequency);
                std::fill(m_current_epoch_max_frequency.begin(),
                          m_current_epoch_max_frequency.end(),
                          m_frequency_min);

                m_last_epoch_time = new_epoch_time;
                m_last_epoch_count = epoch_count;
                m_epoch_wait_count = MINIMUM_EPOCHS_FOR_NEW_EPOCH_CONTROL;
                m_handle_new_epoch = true;
            }
        }
        else if (std::isnan(m_last_epoch_count)) {
            // We don't want to update the previous epoch count if the wait
            // period has not been exceeded. BUT we must update it if the
            // previous count was NaN (e.g., we are observing the first epoch
            // now)
            m_last_epoch_count = epoch_count;
        }
    }

    void FrequencyBalancerAgent::wait(void)
    {
        double wait_seconds = std::max(0.0, M_WAIT_SEC - geopm_time_since(&m_wait_time));
        if (wait_seconds > 0) {
            std::this_thread::sleep_for(
                    std::chrono::duration<double>(wait_seconds));
        }
        geopm_time(&m_wait_time);
    }

    std::vector<std::string> FrequencyBalancerAgent::policy_names(void)
    {
        return { "POWER_PACKAGE_LIMIT_TOTAL",
                 "USE_FREQUENCY_LIMITS", "USE_SST_TF" };
    }

    std::vector<std::string> FrequencyBalancerAgent::sample_names(void)
    {
        return {};
    }

    std::vector<std::pair<std::string, std::string> >
        FrequencyBalancerAgent::report_header(void) const
    {
        return {
            {"Agent uses frequency control", std::to_string(m_policy_use_frequency_limits)},
            {"Agent uses SST-TF", std::to_string(m_use_sst_tf)},
        };
    }

    std::vector<std::pair<std::string, std::string> >
        FrequencyBalancerAgent::report_host(void) const
    {
        return {};
    }

    std::map<uint64_t, std::vector<std::pair<std::string, std::string> > >
        FrequencyBalancerAgent::report_region(void) const
    {
        return {};
    }

    std::vector<std::string> FrequencyBalancerAgent::trace_names(void) const
    {
        std::vector<std::string> names;
        for (size_t core_idx = 0; core_idx < m_last_epoch_non_network_time_diff.size(); ++core_idx) {
            names.push_back(std::string("NON_NET_TIME_PER_EPOCH-core-") + std::to_string(core_idx));
        }
        for (int package_idx = 0; package_idx < m_package_count; ++package_idx) {
            names.push_back(std::string("DESIRED_NON_NETWORK_TIME-package-") + std::to_string(package_idx));
        }
        return names;
    }

    std::vector<std::function<std::string(double)> >
        FrequencyBalancerAgent::trace_formats(void) const
    {
        std::vector<std::function<std::string(double)> > formats;
        for (size_t core_idx = 0; core_idx < m_last_epoch_non_network_time_diff.size(); ++core_idx) {
            formats.push_back(string_format_double);
        }
        for (int package_idx = 0; package_idx < m_package_count; ++package_idx) {
            formats.push_back(string_format_double);
        }
        return formats;
    }

    void FrequencyBalancerAgent::trace_values(std::vector<double> &values)
    {
        values.clear();
        values.insert(values.end(), m_last_epoch_non_network_time_diff.begin(), m_last_epoch_non_network_time_diff.end());
        auto target_times = m_frequency_time_balancer->get_target_times();
        values.insert(values.end(), target_times.begin(), target_times.end());
    }

    void FrequencyBalancerAgent::enforce_policy(const std::vector<double> &policy) const
    {
        if (policy.size() != M_NUM_POLICY) {
            throw Exception("FrequencyBalancerAgent::enforce_policy(): policy vector incorrectly sized.",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
    }

    void FrequencyBalancerAgent::init_platform_io(void)
    {
        m_power_gov->init_platform_io();
        m_freq_governor->set_domain_type(m_frequency_ctl_domain_type);
        m_freq_governor->init_platform_io();
        if (SSTClosGovernor::is_supported(m_platform_io)) {
            m_sst_clos_governor->init_platform_io();
        }
        m_last_ctl_frequency = std::vector<double>(
            m_frequency_control_domain_count, m_frequency_max);
        m_last_ctl_clos = std::vector<double>(
            m_frequency_control_domain_count, SSTClosGovernor::HIGH_PRIORITY);
        m_last_epoch_acnt = std::vector<double>(m_frequency_control_domain_count, NAN);
        m_last_epoch_mcnt = std::vector<double>(m_frequency_control_domain_count, NAN);
        m_last_sample_acnt = std::vector<double>(m_frequency_control_domain_count, NAN);
        m_last_sample_mcnt = std::vector<double>(m_frequency_control_domain_count, NAN);
        m_last_hash = std::vector<double>(m_frequency_control_domain_count, NAN);
        m_last_epoch_frequency = std::vector<double>(m_frequency_control_domain_count, NAN);
        m_current_epoch_max_frequency = std::vector<double>(m_frequency_control_domain_count, m_frequency_min);
        m_last_epoch_max_frequency = std::vector<double>(m_frequency_control_domain_count, NAN);
        m_last_epoch_network_time = std::vector<double>(
            m_frequency_control_domain_count, NAN);
        m_last_epoch_non_network_time_diff = std::vector<double>(
            m_frequency_control_domain_count, NAN);
        m_last_epoch_count = NAN;
        m_last_epoch_time = NAN;
        for (size_t ctl_idx = 0;
             ctl_idx < (size_t)m_frequency_control_domain_count; ++ctl_idx) {
            m_acnt_signal_idx.push_back(m_platform_io.push_signal(
                "MSR::APERF:ACNT", m_frequency_ctl_domain_type, ctl_idx));
            m_mcnt_signal_idx.push_back(m_platform_io.push_signal(
                "MSR::MPERF:MCNT", m_frequency_ctl_domain_type, ctl_idx));
            m_hash_signal_idx.push_back(m_platform_io.push_signal(
                "REGION_HASH", m_frequency_ctl_domain_type, ctl_idx));
            m_hint_signal_idx.push_back(m_platform_io.push_signal(
                "REGION_HINT", m_frequency_ctl_domain_type, ctl_idx));
            m_time_hint_network_idx.push_back(m_platform_io.push_signal(
                "TIME_HINT_NETWORK", m_frequency_ctl_domain_type, ctl_idx));
        }
        m_epoch_signal_idx = m_platform_io.push_signal("EPOCH_COUNT",
                                                       GEOPM_DOMAIN_BOARD, 0);
    }
}
