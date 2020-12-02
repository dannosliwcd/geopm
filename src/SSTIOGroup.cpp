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
#include <cinttypes>
#include <iomanip>
#include <sstream>

#include "Agg.hpp"
#include "Exception.hpp"
#include "Helper.hpp"
#include "MSR.hpp"
#include "MSRFieldSignal.hpp"
#include "PlatformTopo.hpp"
#include "SSTControl.hpp"
#include "SSTIO.hpp"
#include "SSTSignal.hpp"
#include "geopm_debug.hpp"
#include "geopm_topo.h"

namespace geopm
{
    const std::map<std::string, SSTIOGroup::sst_signal_mailbox_raw_s> SSTIOGroup::sst_signal_mbox_info = {
        { "SST::CONFIG_LEVEL",
          { SSTIOGroup::SSTMailboxCommand::TURBO_FREQUENCY, 0x00,
            {{ "LEVEL", { 0x00, 16, 23, 1.0, M_UNITS_NONE,
                            "SST configuration level" } }}
          } },
        { "SST::TURBOFREQ_SUPPORT",
          { SSTIOGroup::SSTMailboxCommand::TURBO_FREQUENCY, 0x01,
            {{ "SUPPORTED", { 0x00, 0, 0, 1.0, M_UNITS_NONE, "SST-TF is supported" } }}
          } },
        { "SST::HIGHPRIORITY_NCORES",
          { SSTIOGroup::SSTMailboxCommand::TURBO_FREQUENCY, 0x10,
            {{ "0", { 0x0000, 0, 7, 1.0, M_UNITS_NONE, "Count of high-priority turbo frequency cores in bucket 0" } },
             { "1", { 0x0000, 8, 15, 1.0, M_UNITS_NONE, "Count of high-priority turbo frequency cores in bucket 1" } },
             { "2", { 0x0000, 16, 23, 1.0, M_UNITS_NONE, "Count of high-priority turbo frequency cores in bucket 2" } },
             { "3", { 0x0000, 24, 31, 1.0, M_UNITS_NONE, "Count of high-priority turbo frequency cores in bucket 3" } },
             { "4", { 0x0100, 0, 7, 1.0, M_UNITS_NONE, "Count of high-priority turbo frequency cores in bucket 4" } },
             { "5", { 0x0100, 8, 15, 1.0, M_UNITS_NONE, "Count of high-priority turbo frequency cores in bucket 5" } },
             { "6", { 0x0100, 16, 23, 1.0, M_UNITS_NONE, "Count of high-priority turbo frequency cores in bucket 6" } },
             { "7", { 0x0100, 24, 31, 1.0, M_UNITS_NONE, "Count of high-priority turbo frequency cores in bucket 7" } }}
          } },
        { "SST::HIGHPRIORITY_FREQUENCY_SSE",
          { SSTIOGroup::SSTMailboxCommand::TURBO_FREQUENCY, 0x11,
            {
             { "0", { 0x000000, 0, 7, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 0 at the SSE license level" } },
             { "1", { 0x000000, 8, 15, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 1 at the SSE license level" } },
             { "2", { 0x000000, 16, 23, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 2 at the SSE license level" } },
             { "3", { 0x000000, 24, 31, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 3 at the SSE license level" } },
             { "4", { 0x000100, 0, 7, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 4 at the SSE license level" } },
             { "5", { 0x000100, 8, 15, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 5 at the SSE license level" } },
             { "6", { 0x000100, 16, 23, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 6 at the SSE license level" } },
             { "7", { 0x000100, 24, 31, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 7 at the SSE license level" } }}
          } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX2",
          { SSTIOGroup::SSTMailboxCommand::TURBO_FREQUENCY, 0x11,
            {{ "0", { 0x010000, 0, 7, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 0 at the AVX2 license level" } },
             { "1", { 0x010000, 8, 15, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 1 at the AVX2 license level" } },
             { "2", { 0x010000, 16, 23, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 2 at the AVX2 license level" } },
             { "3", { 0x010000, 24, 31, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 3 at the AVX2 license level" } },
             { "4", { 0x010100, 0, 7, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 4 at the AVX2 license level" } },
             { "5", { 0x010100, 8, 15, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 5 at the AVX2 license level" } },
             { "6", { 0x010100, 16, 23, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 6 at the AVX2 license level" } },
             { "7", { 0x010100, 24, 31, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 7 at the AVX2 license level" } }}
          } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX512",
          { SSTIOGroup::SSTMailboxCommand::TURBO_FREQUENCY, 0x11,
            {{ "0", { 0x020000, 0, 7, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 0 at the AVX2 license level" } },
             { "1", { 0x020000, 8, 15, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 1 at the AVX2 license level" } },
             { "2", { 0x020000, 16, 23, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 2 at the AVX2 license level" } },
             { "3", { 0x020000, 24, 31, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 3 at the AVX2 license level" } },
             { "4", { 0x020100, 0, 7, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 4 at the AVX2 license level" } },
             { "5", { 0x020100, 8, 15, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 5 at the AVX2 license level" } },
             { "6", { 0x020100, 16, 23, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 6 at the AVX2 license level" } },
             { "7", { 0x020100, 24, 31, 1e8, M_UNITS_HERTZ, "High-priority turbo frequency for bucket 7 at the AVX2 license level" } }}
          } },
        { "SST::LOWPRIORITY_FREQUENCY",
          { SSTIOGroup::SSTMailboxCommand::TURBO_FREQUENCY, 0x12,
            {{ "SSE", { 0x00, 0, 7, 1e8, M_UNITS_HERTZ, "Low-priority turbo frequency at the SSE license level" } },
             { "AVX2", { 0x00, 8, 15, 1e8, M_UNITS_HERTZ, "Low-priority turbo frequency at the AVX2 license level" } },
             { "AVX512", { 0x00, 16, 23, 1e8, M_UNITS_HERTZ, "Low-priority turbo frequency at the AVX512 license level" } }}
          } },
        { "SST::COREPRIORITY_SUPPORT",
          { SSTIOGroup::SSTMailboxCommand::SUPPORT_CAPABILITIES, 0x03,
            {{ "CAPABILITIES", { 0x00, 0, 0, 1.0, M_UNITS_NONE, "SST-CP is supported" } }}
          } }
    };

    const std::map<std::string, SSTIOGroup::sst_control_mailbox_raw_s> SSTIOGroup::sst_control_mbox_info = {
        { "SST::TURBO_ENABLE",
            { SSTIOGroup::SSTMailboxCommand::TURBO_FREQUENCY,
                // Control
                0x02, 0x00 /* N/A */,
                {{ "ENABLE", { 0x01, 16, 16, M_UNITS_NONE, "SST-TF is enabled" } }},
                // Signal
                0x01, 0x00
            },
        },
        { "SST::COREPRIORITY_ENABLE",
            // 0x03 when enabling; 0x01 when disabling
            { SSTIOGroup::SSTMailboxCommand::CORE_PRIORITY,
                // Control
                0x02, 0x100,
                {{ "ENABLE", { 0x01, 1, 1, M_UNITS_NONE, "SST-CP is enabled" } },
                 { "DISABLE_RMID_REPORTING", { 0x01, 0, 0, M_UNITS_NONE, "SST RMID reporting is disabled"  } }},
                // Signal
                0x02, 0x00
            },
        },
    };

    const std::map<std::string, SSTIOGroup::sst_control_mmio_raw_s> SSTIOGroup::sst_control_mmio_info = {
        { "SST::COREPRIORITY:0",
          { GEOPM_DOMAIN_PACKAGE, 0x08,
            { { "WEIGHT", { 4, 7, 1.0, M_UNITS_NONE, "Proportional priority for core priority level 0"  } },
              { "FREQUENCY_MIN", { 8, 15, 1e-8, M_UNITS_HERTZ, "Minimum frequency of core priority level 0"  } },
              { "FREQUENCY_MAX", { 16, 23, 1e-8, M_UNITS_HERTZ, "Maximum frequency of core priority level 0"  } } } } },
        { "SST::COREPRIORITY:1",
          { GEOPM_DOMAIN_PACKAGE, 0x0c,
            { { "WEIGHT", { 4, 7, 1.0, M_UNITS_NONE, "Proportional priority for core priority level 1"  } },
              { "FREQUENCY_MIN", { 8, 15, 1e-8, M_UNITS_HERTZ, "Minimum frequency of core priority level 1"  } },
              { "FREQUENCY_MAX", { 16, 23, 1e-8, M_UNITS_HERTZ, "Maximum frequency of core priority level 1"  } } } } },
        { "SST::COREPRIORITY:2",
          { GEOPM_DOMAIN_PACKAGE, 0x10,
            { { "WEIGHT", { 4, 7, 1.0, M_UNITS_NONE, "Proportional priority for core priority level 2"  } },
              { "FREQUENCY_MIN", { 8, 15, 1e-8, M_UNITS_HERTZ, "Minimum frequency of core priority level 2"  } },
              { "FREQUENCY_MAX", { 16, 23, 1e-8, M_UNITS_HERTZ, "Maximum frequency of core priority level 2"  } } } } },
        { "SST::COREPRIORITY:3",
          { GEOPM_DOMAIN_PACKAGE, 0x14,
            { { "WEIGHT", { 4, 7, 1.0, M_UNITS_NONE, "Proportional priority for core priority level 3"  } },
              { "FREQUENCY_MIN", { 8, 15, 1e-8, M_UNITS_HERTZ, "Minimum frequency of core priority level 3"  } },
              { "FREQUENCY_MAX", { 16, 23, 1e-8, M_UNITS_HERTZ, "Maximum frequency of core priority level 3"  } } } } },
        { "SST::COREPRIORITY",
          { GEOPM_DOMAIN_CORE, 0x20, /* offset will be augmented by core index */
            { { "ASSOCIATION", { 16, 17, 1.0, M_UNITS_NONE, "Assigned core priority level"  } } } } },
    };

    SSTIOGroup::SSTIOGroup(const PlatformTopo &topo, std::shared_ptr<SSTIO> sstio)
        : m_topo(topo)
        , m_sstio(sstio)
        , m_is_read(false)
        , m_signal_available()
        , m_control_available()
        , m_signal_pushed()
        , m_control_pushed()
    {
        if (m_sstio == nullptr) {
            m_sstio = SSTIO::make_shared(topo.num_domain(GEOPM_DOMAIN_CPU));
        }

        // Directly register MBOX-based signals
        for (const auto &kv : sst_signal_mbox_info) {
            auto raw_name = kv.first;
            auto raw_desc = kv.second;

            add_mbox_signals(raw_name, raw_desc.command, raw_desc.subcommand,
                             raw_desc.fields);
        }

        // For MBOX-based controls, register both a control and a signal. The
        // control needs to be aware of how the signal reads are performed so
        // it can do software read/modify/write.
        for (const auto &kv : sst_control_mbox_info) {
            auto raw_name = kv.first;
            auto raw_desc = kv.second;

            // Create a read mask for pre-write reads in the control. The mask
            // is a union of all known fields.
            uint32_t control_read_mask = 0;

            std::map<std::string, sst_signal_mailbox_field_s> fields;
            for (const auto& field : raw_desc.fields)
            {
                fields.emplace(field.first,
                               sst_signal_mailbox_field_s{
                                   raw_desc.read_request_data, field.second.begin_bit,
                                   field.second.end_bit, 1.0, field.second.units,
                                   field.second.description });
                auto bit_count = field.second.end_bit - field.second.begin_bit + 1;
                auto field_mask = ((1ull << bit_count) - 1) << field.second.begin_bit;
                control_read_mask |= field_mask;
            }

            add_mbox_signals(raw_name, raw_desc.command,
                             raw_desc.read_subcommand, fields);
            add_mbox_controls(raw_name, raw_desc.command, raw_desc.subcommand,
                              raw_desc.write_param, raw_desc.fields,
                              raw_desc.read_subcommand,
                              raw_desc.read_request_data, control_read_mask);
        }

        // This IOGroup currently has no MMIO-based signals, except for those
        // that are registered with their related controls below.

        // For MMIO-based controls, register both a control and a signal.
        for (const auto &kv : sst_control_mmio_info) {
            auto raw_name = kv.first;
            auto raw_desc = kv.second;

            uint32_t control_read_mask = 0;

            std::map<std::string, sst_signal_mmio_field_s> fields;
            for (const auto& field : raw_desc.fields)
            {
                fields.emplace(field.first,
                               sst_signal_mmio_field_s{
                                   0, field.second.begin_bit, field.second.end_bit,
                                   1 / field.second.multiplier, field.second.units,
                                   field.second.description });
                auto bit_count = field.second.end_bit - field.second.begin_bit + 1;
                auto field_mask = ((1ull << bit_count) - 1) << field.second.begin_bit;
                control_read_mask |= field_mask;
            }

            add_mmio_signals(raw_name, raw_desc.domain_type, raw_desc.register_offset, fields);
            add_mmio_controls(raw_name, raw_desc.domain_type, raw_desc.register_offset,
                              raw_desc.fields, control_read_mask);
        }
    }

    std::set<std::string> SSTIOGroup::signal_names(void) const
    {
        std::set<std::string> result;
        for (const auto &kv : m_signal_available) {
            result.insert(kv.first);
        }
        return result;
    }

    std::set<std::string> SSTIOGroup::control_names(void) const
    {
        std::set<std::string> s;
        for (const auto &kv : m_control_available) {
            s.insert(kv.first);
        }
        return s;
    }

    bool SSTIOGroup::is_valid_signal(const std::string &signal_name) const
    {
        return (m_signal_available.find(signal_name) != m_signal_available.end());
    }

    bool SSTIOGroup::is_valid_control(const std::string &control_name) const
    {
        return m_control_available.find(control_name) != m_control_available.end();
    }

    int SSTIOGroup::signal_domain_type(const std::string &signal_name) const
    {
        int result = GEOPM_DOMAIN_INVALID;
        auto it = m_signal_available.find(signal_name);
        if (it != m_signal_available.end()) {
            result = it->second.domain;
        }
        return result;
    }

    int SSTIOGroup::control_domain_type(const std::string &control_name) const
    {
        int result = GEOPM_DOMAIN_INVALID;
        auto it = m_control_available.find(control_name);
        if (it != m_control_available.end()) {
            result = it->second.domain;
        }
        return result;
    }

    int SSTIOGroup::push_signal(const std::string &signal_name, int domain_type, int domain_idx)
    {
        int result = -1;
        auto it = m_signal_available.find(signal_name);
        if (it != m_signal_available.end()) {

            if (domain_type != it->second.domain) {
                throw Exception("wrong domain type", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
            auto signal = it->second.signals[domain_idx];
            // TODO: see linear search in MSRIO::push_signal to check for already pushed
            result = m_signal_pushed.size();
            m_signal_pushed.push_back(signal);
            signal->setup_batch();
        }
        else {
            throw Exception("invalid signal", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        // TODO: TBD vector of m_is_signal_pushed = true;
        return result;
    }

    int SSTIOGroup::push_control(const std::string &control_name, int domain_type, int domain_idx)
    {
        int result = -1;
        auto it = m_control_available.find(control_name);
        if (it != m_control_available.end()) {
            if (domain_type != it->second.domain) {
                throw Exception("wrong domain type", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
            auto control = it->second.controls[domain_idx];
            // TODO: see linear search in MSRIO::push_signal to check for already pushed
            result = m_control_pushed.size();
            m_control_pushed.push_back(control);
            control->setup_batch();
        }
        else {
            throw Exception("invalid control", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        return result;
    }

    void SSTIOGroup::read_batch(void)
    {
        m_sstio->read_batch();
        m_is_read = true;
    }

    void SSTIOGroup::write_batch(void)
    {
        m_sstio->write_batch();
    }

    double SSTIOGroup::sample(int batch_idx)
    {
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
    }

    double SSTIOGroup::read_signal(const std::string &signal_name,
                                   int domain_type, int domain_idx)
    {
        // TODO: This needs to call SSTSignal::read();
        auto idx = push_signal(signal_name, domain_type, domain_idx);
        read_batch();
        return sample(idx);

    }

    void SSTIOGroup::write_control(const std::string &control_name,
                                   int domain_type, int domain_idx, double setting)
    {
        // TODO: This needs to call SSTSignal::read();
        auto idx = push_control(control_name, domain_type, domain_idx);
        adjust(idx, setting);
        write_batch();
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
        auto it = m_signal_available.find(signal_name);
        if (it == m_signal_available.end()) {
            throw Exception("SSTIOGroup::signal_description(): " + signal_name +
                                "not valid for SSTIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        std::ostringstream oss;
        oss << "    description: " << it->second.description << "\n"
            << "    units: " << IOGroup::units_to_string(it->second.units) << "\n"
            << "    aggregation: " << Agg::function_to_name(Agg::select_first) << "\n"
            << "    domain: " << platform_topo().domain_type_to_name(it->second.domain) << "\n"
            << "    iogroup: SSTIOGroup";

        return oss.str();
    }

    std::string SSTIOGroup::control_description(const std::string &control_name) const
    {
        auto it = m_control_available.find(control_name);
        if (it == m_control_available.end()) {
            throw Exception("SSTIOGroup::control_description(): " + control_name +
                                "not valid for SSTIOGroup",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }

        std::ostringstream oss;
        oss << "    description: " << it->second.description << "\n"
            << "    units: " << IOGroup::units_to_string(it->second.units) << "\n"
            << "    aggregation: " << Agg::function_to_name(Agg::select_first) << "\n"
            << "    domain: " << platform_topo().domain_type_to_name(it->second.domain) << "\n"
            << "    iogroup: SSTIOGroup";

        return oss.str();
    }

    void SSTIOGroup::add_mbox_signals(
        const std::string &raw_name, SSTIOGroup::SSTMailboxCommand command,
        uint16_t subcommand, std::map<std::string, sst_signal_mailbox_field_s> &fields)

    {
        int domain_type = GEOPM_DOMAIN_PACKAGE;
        int num_domain = m_topo.num_domain(domain_type);

        for (const auto &ff : fields) {
            auto field_name = ff.first;
            auto field_desc = ff.second;
            auto request_data = field_desc.request_data;
            auto begin_bit = field_desc.begin_bit;
            auto end_bit = field_desc.end_bit;
            auto description = field_desc.description;
            auto units = field_desc.units;
            double multiplier = field_desc.multiplier;

            char hex[32];
            snprintf(hex, 32, "0x%05" PRIx32, request_data);
            std::string raw_signal_name = raw_name + "_" + hex + "#";

            // add raw signal for every domain index
            auto it = m_signal_available.find(raw_signal_name);
            if (it == m_signal_available.end()) {
                // TODO: this part of loop can be factored out if each raw SST
                // knows ahead of time the list of request_data values;
                // i.e. if request data was part of the raw struct
                std::vector<std::shared_ptr<Signal> > signals;
                for (int domain_idx = 0; domain_idx < num_domain; ++domain_idx) {
                    // TODO: assumes using any CPU in package is fine
                    auto cpus = m_topo.domain_nested(GEOPM_DOMAIN_CPU, domain_type, domain_idx);
                    int cpu_idx = *(cpus.begin());
                    auto raw_sst = std::make_shared<SSTSignal>(
                        m_sstio, SSTSignal::MBOX, cpu_idx, static_cast<uint16_t>(command),
                        subcommand, request_data, 0 /* interface parameter */);
                    signals.push_back(raw_sst);
                }
                m_signal_available[raw_signal_name] = {
                    .signals = signals,
                    .domain = domain_type,
                    .units = units,
                    .agg_function = Agg::select_first,
                    .description = description
                };
            }
            std::string field_signal_name = raw_name + ":" + field_name;
            auto raw_sst_list = m_signal_available.at(raw_signal_name).signals;
            std::vector<std::shared_ptr<Signal> > signals;
            for (int domain_idx = 0; domain_idx < num_domain; ++domain_idx) {
                auto field_signal = std::make_shared<MSRFieldSignal>(
                    raw_sst_list[domain_idx], begin_bit, end_bit,
                    MSR::M_FUNCTION_SCALE, multiplier);
                signals.push_back(field_signal);
            }
            m_signal_available[field_signal_name] = {
                .signals = signals,
                .domain = domain_type,
                .units = units,
                .agg_function = Agg::select_first,
                .description = description
            };
        }
    }

    void SSTIOGroup::add_mbox_controls(
        const std::string &raw_name, SSTMailboxCommand command,
        uint16_t subcommand, uint32_t write_param,
        const std::map<std::string, sst_control_mailbox_field_s> &fields,
        uint16_t read_subcommand, uint32_t read_request_data, uint32_t read_mask)
    {
        int domain_type = GEOPM_DOMAIN_PACKAGE;
        int num_domain = m_topo.num_domain(domain_type);

        for (const auto &ff : fields) {
            auto field_name = ff.first;
            auto field_description = ff.second;
            // TODO write_data not needed at this time for mbox-type
            // interactions? Only for adjust?
            auto write_data = field_description.write_data;
            auto begin_bit = field_description.begin_bit;
            auto end_bit = field_description.end_bit;
            auto description = field_description.description;
            auto units = field_description.units;

            std::string field_control_name = raw_name + ":" + field_name;

            // add raw control for every domain index
            auto it = m_control_available.find(field_control_name);
            if (it == m_control_available.end()) {
                std::vector<std::shared_ptr<Control> > controls;
                for (int domain_idx = 0; domain_idx < num_domain; ++domain_idx) {
                    auto cpus = m_topo.domain_nested(GEOPM_DOMAIN_CPU,
                                                     domain_type, domain_idx);
                    int cpu_idx = *(cpus.begin());

                    auto raw_sst = std::make_shared<SSTControl>(
                        m_sstio, SSTControl::MBOX, cpu_idx, static_cast<uint16_t>(command),
                        subcommand, write_param, write_data, begin_bit, end_bit,
                        1.0, read_subcommand, read_request_data, read_mask);
                    // TODO: Generate read io params for pre-write.All same except
                    // subcmd and req data. read entire thing instead of per field

                    controls.push_back(raw_sst);
                }
                m_control_available[field_control_name] = {
                    .controls = controls,
                    .domain = domain_type,
                    .units = units,
                    .agg_function = Agg::select_first,
                    .description = description
                };
            }
        }
    }

    void SSTIOGroup::add_mmio_signals(const std::string &raw_name,
                                      int domain_type, uint32_t register_offset,
                                      std::map<std::string, sst_signal_mmio_field_s> &fields)
    {
        int num_domain = m_topo.num_domain(domain_type);

        for (const auto &ff : fields) {
            auto field_name = ff.first;
            auto field_desc = ff.second;
            auto write_value = field_desc.write_value;
            auto begin_bit = field_desc.begin_bit;
            auto end_bit = field_desc.end_bit;
            auto description = field_desc.description;
            auto units = field_desc.units;
            double multiplier = field_desc.multiplier;

            char hex[32];
            snprintf(hex, 32, "0x%05" PRIx32, register_offset);
            std::string raw_signal_name = raw_name + "_" + hex + "#";

            // add raw signal for every domain index
            auto it = m_signal_available.find(raw_signal_name);
            if (it == m_signal_available.end()) {
                std::vector<std::shared_ptr<Signal> > signals;
                for (int domain_idx = 0; domain_idx < num_domain; ++domain_idx) {
                    // TODO: assumes using any CPU in package is fine
                    auto cpus = m_topo.domain_nested(GEOPM_DOMAIN_CPU, domain_type, domain_idx);
                    int cpu_idx = *(cpus.begin());
                    uint32_t augmented_offset = domain_type == GEOPM_DOMAIN_CORE
                        ? register_offset + m_sstio->get_punit_from_cpu(domain_idx) * 4
                        : register_offset;
                    auto raw_sst = std::make_shared<SSTSignal>(
                        m_sstio, SSTSignal::MMIO, cpu_idx, 0x00, 0x00, augmented_offset, write_value);

                    signals.push_back(raw_sst);
                }
                m_signal_available[raw_signal_name] = {
                    .signals = signals,
                    .domain = domain_type,
                    .units = units,
                    .agg_function = Agg::select_first,
                    .description = description
                };
            }

            std::string field_signal_name = raw_name + ":" + field_name;
            auto raw_sst_list = m_signal_available.at(raw_signal_name).signals;
            std::vector<std::shared_ptr<Signal> > signals;
            for (int domain_idx = 0; domain_idx < num_domain; ++domain_idx) {
                auto field_signal = std::make_shared<MSRFieldSignal>(
                    raw_sst_list[domain_idx], begin_bit, end_bit,
                    MSR::M_FUNCTION_SCALE, multiplier);
                signals.push_back(field_signal);
            }
            m_signal_available[field_signal_name] = {
                .signals = signals,
                .domain = domain_type,
                .units = units,
                .agg_function = Agg::select_first,
                .description = description
            };
        }
    }

    void SSTIOGroup::add_mmio_controls(
        const std::string &raw_name, int domain_type, uint32_t register_offset,
        const std::map<std::string, sst_control_mmio_field_s> &fields, uint32_t read_mask)
    {
        int num_domain = m_topo.num_domain(domain_type);

        for (const auto &ff : fields) {
            auto field_name = ff.first;
            auto field_desc = ff.second;
            auto begin_bit = field_desc.begin_bit;
            auto end_bit = field_desc.end_bit;
            auto description = field_desc.description;
            auto units = field_desc.units;
            double multiplier = field_desc.multiplier;

            // add raw control for every domain index
            std::string raw_control_name = raw_name + ":" + field_name;
            auto it = m_control_available.find(raw_control_name);
            if (it == m_control_available.end()) {
                std::vector<std::shared_ptr<Control> > controls;
                for (int domain_idx = 0; domain_idx < num_domain; ++domain_idx) {
                    // TODO: assumes using any CPU in package is fine
                    auto cpus = m_topo.domain_nested(GEOPM_DOMAIN_CPU, domain_type, domain_idx);
                    int cpu_idx = *(cpus.begin());
                    uint32_t augmented_offset = domain_type == GEOPM_DOMAIN_CORE
                        ? register_offset + m_sstio->get_punit_from_cpu(domain_idx) * 4
                        : register_offset;
                    auto control = std::make_shared<SSTControl>(
                        m_sstio, SSTControl::MMIO, cpu_idx, 0x00, 0x00, augmented_offset,
                        0x00 /* Write value. adjust later */, begin_bit,
                        end_bit, multiplier, 0x00, 0x00, read_mask);

                    controls.push_back(control);
                }
                m_control_available[raw_control_name] = {
                    .controls = controls,
                    .domain = domain_type,
                    .units = units,
                    .agg_function = Agg::select_first,
                    .description = description
                };
            }
        }
    }
}
