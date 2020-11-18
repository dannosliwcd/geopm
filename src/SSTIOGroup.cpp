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
    // TODO: do we want JSON config like for MSRs?
    enum class SSTMailboxCommand : uint16_t {
        TURBO_FREQUENCY = 0x7f,
        CORE_PRIORITY = 0xd0,
        SUPPORT_CAPABILITIES = 0x94,
    };

    struct sst_signal_mailbox_field_s {
        //! Fields for an SST mailbox signal command
        //! @param request_data Data to write to the mailbox prior to
        //!        requesting new data. Often used to indicate which data to
        //!        request for a given subcommand.
        //! @param begin_bit LSB position to read from the output value.
        //! @param end_bit MSB position to read from the output value.
        //! @param multiplier Scaling factor to apply to the read value.
        sst_signal_mailbox_field_s(uint32_t request_data,
                                    uint32_t begin_bit, uint32_t end_bit,
                                    double multiplier)
            : request_data(request_data)
            , begin_bit(begin_bit)
            , end_bit(end_bit)
            , multiplier(multiplier)
        {
        }
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
    struct sst_signal_mailbox_raw_s {
        //! @param command Which type of mailbox command
        //! @param subcommand Subtype of the given command
        //! @param fields Subfields of the mailbox
        sst_signal_mailbox_raw_s(SSTMailboxCommand command, uint16_t subcommand,
                                 std::map<std::string, sst_signal_mailbox_field_s> fields)
            : command(command)
            , subcommand(subcommand)
            , fields(fields)
        {
        }
        SSTMailboxCommand command;
        uint16_t subcommand;
        std::map<std::string, sst_signal_mailbox_field_s> fields;
    };


    //// Controls
    struct sst_control_mailbox_fields_s {
        sst_control_mailbox_fields_s(SSTMailboxCommand command, uint16_t subcommand,
                                     uint32_t write_param, uint32_t write_data,
                                     uint32_t begin_bit, uint32_t end_bit)
            : command(command)
            , subcommand(subcommand)
            , write_param(write_param)
            , write_data(write_data)
            , begin_bit(begin_bit)
            , end_bit(end_bit)
        {
        }
        SSTMailboxCommand command;
        uint16_t subcommand;
        uint32_t write_param;
        uint32_t write_data;
        uint32_t begin_bit;
        uint32_t end_bit;
    };

    /// MMIO - no signals
    struct sst_signal_mmio_fields_s {
        sst_signal_mmio_fields_s(uint32_t register_offset, uint32_t write_value,
                                 uint32_t begin_bit, uint32_t end_bit)
            : register_offset(register_offset)
            , write_value(write_value)
            , begin_bit(begin_bit)
            , end_bit(end_bit)
        {
        }
        uint32_t register_offset;
        uint32_t write_value;
        uint32_t begin_bit;
        uint32_t end_bit;
    };

    struct sst_control_mmio_fields_s {
        sst_control_mmio_fields_s(uint32_t register_offset, uint32_t begin_bit,
                                  uint32_t end_bit, double multiplier)
            : register_offset(register_offset)
            , begin_bit(begin_bit)
            , end_bit(end_bit)
            , multiplier(multiplier)
        {
        }
        uint32_t register_offset;
        uint32_t begin_bit;
        uint32_t end_bit;
        double multiplier;
    };

    static const std::map<std::string, sst_signal_mailbox_raw_s> sst_signal_mbox_info = {
        { "SST::CONFIG_LEVEL",
          { SSTMailboxCommand::TURBO_FREQUENCY, 0x00,
            {{ "LEVEL", {0x00, 16, 23, 1.0 } }}
          } },
        { "SST::TURBOFREQ_SUPPORT",
          { SSTMailboxCommand::TURBO_FREQUENCY, 0x01,
            {{ "SUPPORTED", { 0x00, 0, 0, 1.0 } }}
          } },
        // TODO: alias: TURBOFREQ_ENABLE?
        { "SST::TURBO_ENABLE",
          { SSTMailboxCommand::TURBO_FREQUENCY, 0x01,
            {{ "ENABLE", { 0x00, 16, 16, 1.0 } }}
          } },
        // TODO: Add an alias: COREPRIORITY_STATUS?
        { "SST::COREPRIORITY_ENABLE",
          { SSTMailboxCommand::CORE_PRIORITY, 0x02,
            {{ "ENABLE", { 0x00, 1, 1, 1.0 } }}
          } },
        { "SST::HIGHPRIORITY_NCORES",
          { SSTMailboxCommand::TURBO_FREQUENCY, 0x10,
            {{ "0", { 0x0000, 0, 7, 1.0 } },
             { "1", { 0x0000, 8, 15, 1.0 } },
             { "2", { 0x0000, 16, 23, 1.0 } },
             { "3", { 0x0000, 24, 31, 1.0 } },
             { "4", { 0x0100, 0, 7, 1.0 } },
             { "5", { 0x0100, 8, 15, 1.0 } },
             { "6", { 0x0100, 16, 23, 1.0 } },
             { "7", { 0x0100, 24, 31, 1.0 } }}
          } },
        { "SST::HIGHPRIORITY_FREQUENCY_SSE",
          { SSTMailboxCommand::TURBO_FREQUENCY, 0x11,
            {
             { "0", { 0x000000, 0, 7, 1e8 } },
             { "1", { 0x000000, 8, 15, 1e8 } },
             { "2", { 0x000000, 16, 23, 1e8 } },
             { "3", { 0x000000, 24, 31, 1e8 } },
             { "4", { 0x000100, 0, 7, 1e8 } },
             { "5", { 0x000100, 8, 15, 1e8 } },
             { "6", { 0x000100, 16, 23, 1e8 } },
             { "7", { 0x000100, 24, 31, 1e8 } }}
          } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX2",
          { SSTMailboxCommand::TURBO_FREQUENCY, 0x11,
            {{ "0", { 0x010000, 0, 7, 1e8 } },
             { "1", { 0x010000, 8, 15, 1e8 } },
             { "2", { 0x010000, 16, 23, 1e8 } },
             { "3", { 0x010000, 24, 31, 1e8 } },
             { "4", { 0x010100, 0, 7, 1e8 } },
             { "5", { 0x010100, 8, 15, 1e8 } },
             { "6", { 0x010100, 16, 23, 1e8 } },
             { "7", { 0x010100, 24, 31, 1e8 } }}
          } },
        { "SST::HIGHPRIORITY_FREQUENCY_AVX512",
          { SSTMailboxCommand::TURBO_FREQUENCY, 0x11,
            {{ "0", { 0x020000, 0, 7, 1e8 } },
             { "1", { 0x020000, 8, 15, 1e8 } },
             { "2", { 0x020000, 16, 23, 1e8 } },
             { "3", { 0x020000, 24, 31, 1e8 } },
             { "4", { 0x020100, 0, 7, 1e8 } },
             { "5", { 0x020100, 8, 15, 1e8 } },
             { "6", { 0x020100, 16, 23, 1e8 } },
             { "7", { 0x020100, 24, 31, 1e8 } }}
          } },
        { "SST::LOWPRIORITY_FREQUENCY",
          { SSTMailboxCommand::TURBO_FREQUENCY, 0x12,
            {{ "SSE", { 0x00, 0, 7, 1e8 } },
             { "AVX2", { 0x00, 8, 15, 1e8 } },
             { "AVX512", { 0x00, 16, 23, 1e8 } }}
          } },
        { "SST::COREPRIORITY_SUPPORT",
          { SSTMailboxCommand::SUPPORT_CAPABILITIES, 0x03,
            {{ "CAPABILITIES", { 0x00, 0, 0, 1.0 } }}
          } }
    };

    static const std::map<std::string, sst_control_mailbox_fields_s> sst_control_mbox_fields = {
        { "SST::TURBO_ENABLE",
          { SSTMailboxCommand::TURBO_FREQUENCY, 0x02, 0x00 /* N/A */, 0x01, 16, 16 } },
        { "SST::COREPRIORITY_ENABLE",
          { SSTMailboxCommand::CORE_PRIORITY, 0x00, 0x1000000, 0x01, 17, 17 } },
    };

    static const std::map<std::string, sst_signal_mmio_fields_s> sst_signal_mmio_fields = {
        //{ "SST::COREPRIORITY_SUPPORT", { 0x00, 0x00, 0, 7 } },
    };

    static const std::map<std::string, sst_control_mmio_fields_s> sst_control_mmio_fields = {
        { "SST::COREPRIORITY_WEIGHT_0", { 0x08, 4, 7, 1.0 } },
        { "SST::COREPRIORITY_WEIGHT_1", { 0x0c, 4, 7, 1.0 } },
        { "SST::COREPRIORITY_WEIGHT_2", { 0x10, 4, 7, 1.0 } },
        { "SST::COREPRIORITY_WEIGHT_3", { 0x14, 4, 7, 1.0 } },
        { "SST::FREQUENCY_MIN_0", { 0x08, 8, 15, 1e-8 } },
        { "SST::FREQUENCY_MIN_1", { 0x0c, 8, 15, 1e-8 } },
        { "SST::FREQUENCY_MIN_2", { 0x10, 8, 15, 1e-8 } },
        { "SST::FREQUENCY_MIN_3", { 0x14, 8, 15, 1e-8 } },
        { "SST::FREQUENCY_MAX_0", { 0x08, 16, 23, 1e-8 } },
        { "SST::FREQUENCY_MAX_1", { 0x0c, 16, 23, 1e-8 } },
        { "SST::FREQUENCY_MAX_2", { 0x10, 16, 23, 1e-8 } },
        { "SST::FREQUENCY_MAX_3", { 0x14, 16, 23, 1e-8 } },
        // TODO:
        // -- add some kind of thing to tell geopmread to modify 0x20+4*phys_core_id
        // -- (Current impl of this ctl only prints core 0)
    };

    SSTIOGroup::SSTIOGroup(const PlatformTopo &topo, std::shared_ptr<SSTIO> sstio)
        : m_topo(topo)
        , m_sstio(sstio)
        , m_is_read(false)
        , m_signal_available()
        , m_signal_pushed()
        , m_control_pushed()
    {
        if (m_sstio == nullptr) {
            m_sstio = SSTIO::make_shared(topo.num_domain(GEOPM_DOMAIN_CPU));
        }

        // TODO: might want to replace some autos with types
        for (const auto &kv : sst_signal_mbox_info) {
            auto raw_name = kv.first;
            auto raw_desc = kv.second;
            auto command = raw_desc.command;
            auto subcommand = raw_desc.subcommand;
            auto fields = raw_desc.fields;

            int domain_type = GEOPM_DOMAIN_PACKAGE;
            int num_domain = m_topo.num_domain(domain_type);

            for (const auto &ff : fields) {
                auto field_name = ff.first;
                auto field_desc = ff.second;
                auto request_data = field_desc.request_data;
                auto begin_bit = field_desc.begin_bit;
                auto end_bit = field_desc.end_bit;
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
                            m_sstio, false, cpu_idx, static_cast<uint16_t>(command),
                            subcommand, request_data, 0 /* interface parameter */);
                        signals.push_back(raw_sst);
                    }
                    m_signal_available[raw_signal_name] = {
                        .signals = signals,
                        .domain = GEOPM_DOMAIN_PACKAGE,
                        .units = IOGroup::M_UNITS_NONE,
                        .agg_function = Agg::select_first,
                        .description = "TODO"
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
                    .domain = GEOPM_DOMAIN_PACKAGE,
                    .units = IOGroup::M_UNITS_NONE,
                    .agg_function = Agg::select_first,
                    .description = "TODO"
                };
            }
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
        std::transform(sst_control_mbox_fields.begin(), sst_control_mbox_fields.end(),
                       std::inserter(s, s.end()),
                       [](const decltype(sst_control_mbox_fields)::value_type& p) {
                           return p.first;
                       });
        std::transform(sst_control_mmio_fields.begin(), sst_control_mmio_fields.end(),
                       std::inserter(s, s.end()),
                       [](const decltype(sst_control_mmio_fields)::value_type& p) {
                           return p.first;
                       });
        s.insert("SST::COREPRIORITY_ASSOCIATION");
        return s;
    }

    bool SSTIOGroup::is_valid_signal(const std::string &signal_name) const
    {
        return (m_signal_available.find(signal_name) != m_signal_available.end());
    }

    bool SSTIOGroup::is_valid_control(const std::string &control_name) const
    {
        return control_name == "SST::COREPRIORITY_ASSOCIATION" ||
               (sst_control_mbox_fields.find(control_name) !=
                sst_control_mbox_fields.end()) ||
               (sst_control_mmio_fields.find(control_name) !=
                sst_control_mmio_fields.end());
    }

    int SSTIOGroup::signal_domain_type(const std::string &signal_name) const
    {
        // TODO: use struct?
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
            result = control_name == "SST::COREPRIORITY_ASSOCIATION"
                         ? GEOPM_DOMAIN_CORE
                         : GEOPM_DOMAIN_PACKAGE;
        }
        return result;
    }

    int SSTIOGroup::push_signal(const std::string &signal_name, int domain_type, int domain_idx)
    {
        int result = -1;
        // TODO: TBD: per-signal domain type and is batch read
        auto it = m_signal_available.find(signal_name);
        auto mmio_it = sst_signal_mmio_fields.find(signal_name);
        if (it != m_signal_available.end()) {

            // TODO: check *it.domain
            if (domain_type != GEOPM_DOMAIN_PACKAGE) {
                throw Exception("wrong domain type", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
            auto signal = it->second.signals[domain_idx];
            // TODO: see linear search in MSRIO::push_signal to check for already pushed
            result = m_signal_pushed.size();
            m_signal_pushed.push_back(signal);
            signal->setup_batch();
        }
        else if (mmio_it != sst_signal_mmio_fields.end()) {
            const auto &field_description = mmio_it->second;
            if (domain_type != GEOPM_DOMAIN_PACKAGE) {
                throw Exception("wrong domain type", GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }

            // TODO: assumes using any CPU in package is fine
            auto cpus = m_topo.domain_nested(GEOPM_DOMAIN_CPU, domain_type, domain_idx);
            int cpu_idx = *(cpus.begin());
            // TODO: need to fix this for the case where multiple signals
            // come from different fields of the same SSTSignal; in that case
            // we should reuse the object
            std::shared_ptr<Signal> signal = std::make_shared<MSRFieldSignal>(
                std::make_shared<SSTSignal>(
                    m_sstio, true, cpu_idx, 0x00, 0x00, field_description.register_offset,
                    field_description.write_value),
                field_description.begin_bit, field_description.end_bit,
                MSR::M_FUNCTION_SCALE, 1.0);

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
        auto mbox_it = sst_control_mbox_fields.find(control_name);
        auto mmio_it = sst_control_mmio_fields.find(control_name);
        if (mbox_it != sst_control_mbox_fields.end()) {
            const auto& field_description = mbox_it->second;
            if (domain_type != GEOPM_DOMAIN_PACKAGE) {
                throw Exception("wrong domain type", GEOPM_ERROR_INVALID,
                                __FILE__, __LINE__);
            }
            // TODO: assumes using any CPU in package is fine
            auto cpus = m_topo.domain_nested(GEOPM_DOMAIN_CPU, domain_type, domain_idx);
            int cpu_idx = *(cpus.begin());

            auto control = std::make_shared<SSTControl>(
                m_sstio, false, cpu_idx, static_cast<uint16_t>(field_description.command),
                field_description.subcommand, field_description.write_param,
                field_description.write_data, field_description.begin_bit,
                field_description.end_bit);
            control->setup_batch();
            result = m_control_pushed.size();
            m_control_pushed.push_back(control);
        }
        else if (mmio_it != sst_control_mmio_fields.end()) {
            const auto& field_description = mmio_it->second;
            if (domain_type != GEOPM_DOMAIN_PACKAGE) {
                throw Exception("wrong domain type", GEOPM_ERROR_INVALID,
                                __FILE__, __LINE__);
            }
            // TODO: assumes using any CPU in package is fine
            auto cpus = m_topo.domain_nested(GEOPM_DOMAIN_CPU, domain_type, domain_idx);
            int cpu_idx = *(cpus.begin());

            // TODO: boolean arg and command/subcommand could probably be
            // removed either with interface-specific functions or separate SST
            // objects for different interfaces.
            auto control = std::make_shared<SSTControl>(
                m_sstio, true, cpu_idx, 0x00, 0x00, field_description.register_offset,
                0x00 /* Write value. adjust later */,
                field_description.begin_bit, field_description.end_bit);
            control->setup_batch();
            result = m_control_pushed.size();
            m_control_pushed.push_back(control);
        }
        else if (control_name == "SST::COREPRIORITY_ASSOCIATION") {
            if (domain_type != GEOPM_DOMAIN_CORE) {
                throw Exception("wrong domain type", GEOPM_ERROR_INVALID,
                                __FILE__, __LINE__);
            }
            auto cpus = m_topo.domain_nested(GEOPM_DOMAIN_CPU, domain_type, domain_idx);
            int cpu_idx = *(cpus.begin());

            geopm::sst_control_mmio_fields_s field_description{
                static_cast<uint32_t>(0x20 + m_sstio->get_punit_from_cpu(domain_idx) * 4),
                16, 17, 1.0
            };
            auto control = std::make_shared<SSTControl>(
                m_sstio, true, cpu_idx, 0x00, 0x00, field_description.register_offset,
                0x00 /* Write value. adjust later */,
                field_description.begin_bit, field_description.end_bit);
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
