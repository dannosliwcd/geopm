/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "MonitorAgent.hpp"

#include "PlatformIOProf.hpp"
#include "ProcessRegionAggregator.hpp"
#include "geopm_topo.h"
#include "record.hpp"
#include "ApplicationSampler.hpp"
#include "geopm/PlatformIO.hpp"
#include "geopm/PlatformTopo.hpp"
#include "geopm/Helper.hpp"
#include "geopm/Exception.hpp"
#include "config.h"
#include <iostream>

namespace geopm
{
    MonitorAgent::MonitorAgent()
        : MonitorAgent(PlatformIOProf::platform_io(), platform_topo())
    {

    }

    MonitorAgent::MonitorAgent(PlatformIO &plat_io, const PlatformTopo &topo)
        : m_app_sampler(ApplicationSampler::application_sampler())
        , m_region_aggregator(ProcessRegionAggregator::make_unique())
        , m_network_hints_set(m_app_sampler.region_hash_network())
        , m_network_hints(m_network_hints_set.begin(), m_network_hints_set.end())
        , m_last_wait(time_zero())
        , m_network_time_per_core(platform_topo().num_domain(GEOPM_DOMAIN_CORE))
        , m_network_count_per_core(platform_topo().num_domain(GEOPM_DOMAIN_CORE))
        , M_WAIT_SEC(0.005)
    {
        geopm_time(&m_last_wait);
    }

    std::string MonitorAgent::plugin_name(void)
    {
        return "monitor";
    }

    std::unique_ptr<Agent> MonitorAgent::make_plugin(void)
    {
        return geopm::make_unique<MonitorAgent>();
    }

    void MonitorAgent::init(int level, const std::vector<int> &fan_in, bool is_level_root)
    {

    }

    void MonitorAgent::validate_policy(std::vector<double> &policy) const
    {

    }

    void MonitorAgent::split_policy(const std::vector<double> &in_policy,
                                    std::vector<std::vector<double> > &out_policy)
    {

    }

    bool MonitorAgent::do_send_policy(void) const
    {
        return false;
    }

    void MonitorAgent::aggregate_sample(const std::vector<std::vector<double> > &in_sample,
                                        std::vector<double> &out_sample)
    {

    }

    bool MonitorAgent::do_send_sample(void) const
    {
        return false;
    }

    void MonitorAgent::adjust_platform(const std::vector<double> &in_policy)
    {

    }

    bool MonitorAgent::do_write_batch(void) const
    {
        return false;
    }

    void MonitorAgent::sample_platform(std::vector<double> &out_sample)
    {
        static_cast<void>(out_sample);
        m_region_aggregator->update();
        auto runtime_by_process = m_region_aggregator->get_runtime_totals(m_network_hints);
        auto counts_by_process = m_region_aggregator->get_count_totals(m_network_hints);

        auto cpu_processes = m_app_sampler.per_cpu_process(); // index=cpu, value=process
        for (size_t cpu_idx = 0; cpu_idx < cpu_processes.size(); ++cpu_idx) {
            int process = cpu_processes[cpu_idx];
            auto core = platform_topo().domain_idx(GEOPM_DOMAIN_CORE, cpu_idx);
            m_network_time_per_core[core] = runtime_by_process[process];
            m_network_count_per_core[core] = counts_by_process[process];
        }
    }

    void MonitorAgent::wait(void)
    {
        while(geopm_time_since(&m_last_wait) < M_WAIT_SEC) {

        }
        geopm_time(&m_last_wait);
    }

    std::vector<std::string> MonitorAgent::policy_names(void)
    {
        return {};
    }

    std::vector<std::string> MonitorAgent::sample_names(void)
    {
        return {};
    }

    std::vector<std::pair<std::string, std::string> > MonitorAgent::report_header(void) const
    {
        std::vector<std::pair<std::string, std::string> > key_value_pairs;
        for (size_t i = 0; i < m_network_time_per_core.size(); ++i) {
            key_value_pairs.emplace_back("TIME_MYHINT_NETWORK@core-" + std::to_string(i),
                    std::to_string(m_network_time_per_core[i]));
            key_value_pairs.emplace_back("COUNT_MYHINT_NETWORK@core-" + std::to_string(i),
                    std::to_string(m_network_count_per_core[i]));
        }
        return key_value_pairs;
    }

    std::vector<std::pair<std::string, std::string> > MonitorAgent::report_host(void) const
    {
        return {};
    }

    std::map<uint64_t, std::vector<std::pair<std::string, std::string> > > MonitorAgent::report_region(void) const
    {
        return {};
    }

    std::vector<std::string> MonitorAgent::trace_names(void) const
    {
        return {};
    }

    std::vector<std::function<std::string(double)> > MonitorAgent::trace_formats(void) const
    {
        return {};
    }

    void MonitorAgent::trace_values(std::vector<double> &values)
    {

    }

    void MonitorAgent::enforce_policy(const std::vector<double> &policy) const
    {

    }
}
