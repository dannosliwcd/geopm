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

#include <memory>

#include "Control.hpp"

namespace geopm
{
    class SSTIO;

    class SSTControl : public geopm::Control
    {
        public:
            SSTControl(std::shared_ptr<SSTIO> sstio, bool is_mmio,
                       int cpu_idx, uint32_t command, uint32_t subcommand,
                       uint32_t interface_parameter, uint32_t write_value,
                       int begin_bit, int end_bit);
            virtual ~SSTControl() = default;
            void setup_batch(void) override;
            void adjust(double value) override;
            void write(double value) override;
            void save(void) override;
            void restore(void) override;
            // static SSTControl make_sstmmio_control(... )
            // static SSTControl make_sstmbox_control (...)
        private:
            std::shared_ptr<SSTIO> m_sstio;
            const bool m_is_mmio;
            const int m_cpu_idx;
            const uint32_t m_command;
            const uint32_t m_subcommand;
            const uint32_t m_interface_parameter;
            const uint32_t m_write_value;
            const int m_shift;
            int m_adjust_idx;

    };
}
