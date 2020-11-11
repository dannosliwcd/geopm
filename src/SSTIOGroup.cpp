/*
 * Copyright (c) 2015, 2016, 2017, 2018, 2019, 2020, Intel Corporation
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
#include "SSTIOGroup.hpp"

#include <algorithm>


#include "geopm_debug.hpp"
#include "PlatformTopo.hpp"
#include "Helper.hpp"
#include "Exception.hpp"
#include "Agg.hpp"
#include "MSR.hpp"
#include "MSRFieldSignal.hpp"
#include "SSTIO.hpp"
#include "SSTSignal.hpp"
#include "SSTControl.hpp"

namespace geopm
{
    struct sst_mailbox_fields_s {
        uint16_t command;
        uint16_t subcommand;
        /* TODO: uint32_t request_data won't work alone.
         * Most, but not all, fields have config_level encoded into bits [7:0].
         * Current version of this iogroup ignores that, effectivly using 0 for all.
         *  -- Add bool has_config_level? Could auto-set in internal layers
         *  -- Add additional signal names (multiply signal count by 4)
         */
        uint32_t request_data;
        uint32_t begin_bit;
        uint32_t end_bit;
        double multiplier;
    };

    static const std::map<std::string, sst_mailbox_fields_s> sst_signal_mbox_fields = {
        { "SST::CONFIG_LEVEL", { 0x7f, 0x00, 0x00, 16, 23 } },
        { "SST::TURBOFREQ_SUPPORT", { 0x7f, 0x01, 0x00, 0, 0 } },
        { "SST::TURBOFREQ_STATUS", { 0x7f, 0x01, 0x00, 16, 16 } },
        // TODO: Add an alias: COREPRIORITY_STATUS?
        { "SST::COREPRIORITY_ENABLE", { 0xd0, 0x02, 0x00, 1, 1, 1.0 } },
        { "SST::HIGHPRIORITY_NCORES_0", { 0x7f, 0x10, 0x0000, 0, 7, 1.0 } },
        { "SST::HIGHPRIORITY_NCORES_1", { 0x7f, 0x10, 0x0000, 8, 15, 1.0 } },
        { "SST::HIGHPRIORITY_NCORES_2", { 0x7f, 0x10, 0x0000, 16, 23, 1.0 } },
        { "SST::HIGHPRIORITY_NCORES_3", { 0x7f, 0x10, 0x0000, 24, 31, 1.0 } },
        { "SST::HIGHPRIORITY_NCORES_4", { 0x7f, 0x10, 0x0100, 0, 7, 1.0 } },
        { "SST::HIGHPRIORITY_NCORES_5", { 0x7f, 0x10, 0x0100, 8, 15, 1.0 } },
        { "SST::HIGHPRIORITY_NCORES_6", { 0x7f, 0x10, 0x0100, 16, 23, 1.0 } },
        { "SST::HIGHPRIORITY_NCORES_7", { 0x7f, 0x10, 0x0100, 24, 31, 1.0 } },
        { "SST::HIGHPRIORITY_FREQUENCY_SSE_0", { 0x7f, 0x11, 0x000000, 23, 31, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_SSE_1", { 0x7f, 0x11, 0x000000, 16, 24, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_SSE_2", { 0x7f, 0x11, 0x000000, 8, 15, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_SSE_3", { 0x7f, 0x11, 0x000000, 0, 7, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_SSE_4", { 0x7f, 0x11, 0x000100, 23, 31, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_SSE_5", { 0x7f, 0x11, 0x000100, 16, 24, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_SSE_6", { 0x7f, 0x11, 0x000100, 8, 15, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_SSE_7", { 0x7f, 0x11, 0x000100, 0, 7, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX2_0", { 0x7f, 0x11, 0x010000, 23, 31, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX2_1", { 0x7f, 0x11, 0x010000, 16, 24, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX2_2", { 0x7f, 0x11, 0x010000, 8, 15, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX2_3", { 0x7f, 0x11, 0x010000, 0, 7, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX2_4", { 0x7f, 0x11, 0x010100, 23, 31, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX2_5", { 0x7f, 0x11, 0x010100, 16, 24, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX2_6", { 0x7f, 0x11, 0x010100, 8, 15, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX2_7", { 0x7f, 0x11, 0x010100, 0, 7, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX512_0", { 0x7f, 0x11, 0x020000, 23, 31, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX512_1", { 0x7f, 0x11, 0x020000, 16, 24, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX512_2", { 0x7f, 0x11, 0x020000, 8, 15, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX512_3", { 0x7f, 0x11, 0x020000, 0, 7, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX512_4", { 0x7f, 0x11, 0x020100, 23, 31, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX512_5", { 0x7f, 0x11, 0x020100, 16, 24, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX512_6", { 0x7f, 0x11, 0x020100, 8, 15, 1e8 } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX512_7", { 0x7f, 0x11, 0x020100, 0, 7, 1e8 } },
        { "SST::LOWPRIORITY_FREQUENCY_SSE", { 0x7f, 0x12, 0x00, 0, 7, 1e8 } },
        { "SST::LOWPRIORITY_FREQUENCY_AVX2", { 0x7f, 0x12, 0x00, 8, 15, 1e8 } },
        { "SST::LOWPRIORITY_FREQUENCY_AVX512", { 0x7f, 0x12, 0x00, 16, 24, 1e8 } },
    };
    // TODO: WIP: Need to make the functions use this. Probably need to add in
    // the mbox param field too.
    static const std::map<std::string, sst_mailbox_fields_s> sst_control_mbox_fields = {
        { "SST::TURBO_ENABLE", { 0x7f, 0x00, 0x00, 16, 23 } },
        { "SST::COREPRIORITY_ENABLE", { 0xd0, 0x02, 0x00, 1, 1, 1.0 } },
    };

    SSTIOGroup::SSTIOGroup(const PlatformTopo &topo,
                           std::shared_ptr<SSTIO> sstio)
        : m_is_signal_pushed(false)
        , m_is_batch_read(false)
        , m_valid_control_name({"SST::TURBO_ENABLE"})
        , m_topo(topo)
        , m_sstio(sstio)
        , m_is_read(false)
    {
        if (m_sstio == nullptr) {
            m_sstio = SSTIO::make_shared();
        }
    }

    std::set<std::string> SSTIOGroup::signal_names(void) const
    {
        std::set<std::string> s;
        std::transform(sst_signal_mbox_fields.begin(), sst_signal_mbox_fields.end(),
                       std::inserter(s, s.end()),
                       [](const decltype(sst_signal_mbox_fields)::value_type& p) {
                           return p.first;
                       });
        return s;
    }

    std::set<std::string> SSTIOGroup::control_names(void) const
    {
        return m_valid_control_name;
    }

    bool SSTIOGroup::is_valid_signal(const std::string &signal_name) const
    {
        return sst_signal_mbox_fields.find(signal_name) != sst_signal_mbox_fields.end();
    }

    bool SSTIOGroup::is_valid_control(const std::string &control_name) const
    {
        return m_valid_control_name.find(control_name) != m_valid_control_name.end();
    }

    int SSTIOGroup::signal_domain_type(const std::string &signal_name) const
    {
        int result = GEOPM_DOMAIN_INVALID;
        if (is_valid_signal(signal_name)) {
            result = GEOPM_DOMAIN_PACKAGE;
        }
        return result;
    }

    int SSTIOGroup::control_domain_type(const std::string &control_name) const
    {
        int result = GEOPM_DOMAIN_INVALID;
        if (is_valid_control(control_name)) {
            result = GEOPM_DOMAIN_PACKAGE;
        }
        return result;
    }

    int SSTIOGroup::push_signal(const std::string &signal_name, int domain_type, int domain_idx)
    {
        int result = -1;
        auto it = sst_signal_mbox_fields.find(signal_name);
        if (it != sst_signal_mbox_fields.end()) {
            const auto& field_description = it->second;
            if (domain_type != GEOPM_DOMAIN_PACKAGE) {
                throw Exception("wrong domain type", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
            // TODO: assumes using any CPU in package is fine
            auto cpus = m_topo.domain_nested(GEOPM_DOMAIN_CPU, domain_type, domain_idx);
            int cpu_idx = *(cpus.begin());
            // "ISST::LEVELS_INFO#"

            std::shared_ptr<Signal> signal = std::make_shared<MSRFieldSignal>(
                std::make_shared<SSTSignal>(
                    m_sstio, cpu_idx, field_description.command,
                    field_description.subcommand, field_description.request_data,
                    0 /* interface parameter */),
                field_description.begin_bit, field_description.end_bit,
                MSR::M_FUNCTION_SCALE, field_description.multiplier);

            // TODO: see linear search in MSRIO::push_signal to check for already pushed
            result = m_signal_pushed.size();
            m_signal_pushed.push_back(signal);
            signal->setup_batch();
        }
        else {
            throw Exception("invalid signal", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        // if (!is_valid_signal(signal_name)) {
        //     throw Exception("SSTIOGroup::push_signal(): signal_name " + signal_name +
        //                     " not valid for SSTIOGroup",
        //                     GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        // }
        // if (domain_type != GEOPM_DOMAIN_CPU) {
        //     throw Exception("SSTIOGroup::push_signal(): signal_name " + signal_name +
        //                     " not defined for domain " + std::to_string(domain_type),
        //                     GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        // }
        // if (m_is_batch_read) {
        //     throw Exception("SSTIOGroup::push_signal(): cannot push signal after call to read_batch().",
        //                     GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        // }
        // m_is_signal_pushed = true;
        // return 0;
        return result;
    }

    int SSTIOGroup::push_control(const std::string &control_name, int domain_type, int domain_idx)
    {
        int result = -1;
        if (is_valid_control(control_name)) {
            if (domain_type != GEOPM_DOMAIN_PACKAGE) {
                throw Exception("wrong domain type", GEOPM_ERROR_INVALID,
                                __FILE__, __LINE__);
            }
            // TODO: assumes using any CPU in package is fine
            auto cpus = m_topo.domain_nested(GEOPM_DOMAIN_CPU, domain_type, domain_idx);
            int cpu_idx = *(cpus.begin());

            auto control = std::make_shared<SSTControl>(m_sstio, cpu_idx, 0x7F, 0x2, 0x0, 0x1, 16, 16);
            control->setup_batch();
            result = m_control_pushed.size();
            m_control_pushed.push_back(control);
        }
        else {
            throw Exception("invalid control", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        return result;
    }

    void SSTIOGroup::read_batch(void)
    {
        // if (m_is_signal_pushed) {
        //     m_time_curr = geopm_time_since(&m_time_zero);
        // }
        m_sstio->read_batch();
        m_is_read = true;
    }

    void SSTIOGroup::write_batch(void)
    {
        m_sstio->write_batch();
    }

    double SSTIOGroup::sample(int batch_idx)
    {
        // if (!m_is_signal_pushed) {
        //     throw Exception("SSTIOGroup::sample(): signal has not been pushed",
        //                     GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        // }
        // if (!m_is_batch_read) {
        //     throw Exception("SSTIOGroup::sample(): signal has not been read",
        //                     GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        // }
        // if (batch_idx != 0) {
        //     throw Exception("SSTIOGroup::sample(): batch_idx out of range",
        //                     GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        // }
        // return m_time_curr;
        if (batch_idx < 0 || batch_idx >= static_cast<int>(m_signal_pushed.size())) {
            throw Exception("SSTIOGroup::sample(): batch_idx out of range",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        if (!m_is_read) {
            throw Exception("SSTIOGroup::sample() called before the signal was read.",
                            GEOPM_ERROR_RUNTIME, __FILE__, __LINE__);
        }
        return m_signal_pushed[batch_idx]->sample();
    }

    void SSTIOGroup::adjust(int batch_idx, double setting)
    {
        m_control_pushed[batch_idx]->adjust(setting);
        // throw Exception("SSTIOGroup::adjust(): there are no controls supported by the SSTIOGroup",
        //                 GEOPM_ERROR_INVALID, __FILE__, __LINE__);
    }

    double SSTIOGroup::read_signal(const std::string &signal_name,
                                   int domain_type, int domain_idx)
    {
        // TODO: This needs to call SSTSignal::read();
        auto idx = push_signal(signal_name, domain_type, domain_idx);
        read_batch();
        return sample(idx);
        // if (!is_valid_signal(signal_name)) {
        //     throw Exception("SSTIOGroup:read_signal(): " + signal_name +
        //                     "not valid for SSTIOGroup",
        //                     GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        // }
        // if (domain_type != GEOPM_DOMAIN_CPU) {
        //     throw Exception("SSTIOGroup::read_signal(): signal_name " + signal_name +
        //                     " not defined for domain " + std::to_string(domain_type),
        //                     GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        // }
        // return geopm_time_since(&m_time_zero);
    }

    void SSTIOGroup::write_control(const std::string &control_name,
                                   int domain_type, int domain_idx, double setting)
    {
        // TODO: This needs to call SSTSignal::read();
        auto idx = push_control(control_name, domain_type, domain_idx);
        adjust(idx, setting);
        write_batch();
        // throw Exception("SSTIOGroup::write_control(): there are no controls
        // supported by the SSTIOGroup",
        //                 GEOPM_ERROR_INVALID, __FILE__, __LINE__);
    }

    void SSTIOGroup::save_control(void)
    {

    }

    void SSTIOGroup::restore_control(void)
    {

    }

    std::string SSTIOGroup::plugin_name(void)
    {
        return "SST";
    }

    std::unique_ptr<IOGroup> SSTIOGroup::make_plugin(void)
    {
        return geopm::make_unique<SSTIOGroup>(platform_topo(), nullptr);
    }

    std::function<double(const std::vector<double> &)> SSTIOGroup::agg_function(const std::string &signal_name) const
    {
        if (!is_valid_signal(signal_name)) {
            throw Exception("SSTIOGroup::agg_function(): " + signal_name +
                            "not valid for SSTIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        return Agg::select_first;
    }

    std::function<std::string(double)> SSTIOGroup::format_function(const std::string &signal_name) const
    {
        if (!is_valid_signal(signal_name)) {
            throw Exception("SSTIOGroup::format_function(): " + signal_name +
                            "not valid for SSTIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        return string_format_double;
    }

    std::string SSTIOGroup::signal_description(const std::string &signal_name) const
    {
        if (!is_valid_signal(signal_name)) {
            throw Exception("SSTIOGroup::signal_description(): " + signal_name +
                            "not valid for SSTIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        std::string result = "Invalid signal description: no description found.";
        result = "    description: Time since the start of application profiling.\n";
        result += "    units: " + IOGroup::units_to_string(M_UNITS_SECONDS) + '\n';
        result += "    aggregation: " + Agg::function_to_name(Agg::select_first) + '\n';
        result += "    domain: " + platform_topo().domain_type_to_name(GEOPM_DOMAIN_CPU) + '\n';
        result += "    iogroup: SSTIOGroup";

        return result;
    }

    std::string SSTIOGroup::control_description(const std::string &control_name) const
    {
        if (!is_valid_control(control_name)) {
            throw Exception("SSTIOGroup::control_description(): " + control_name +
                            "not valid for SSTIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        std::string result = "Invalid control description: no description found "
            "(description is not yet implemented for SSTIOGroup).";

        return result;
    }
}
