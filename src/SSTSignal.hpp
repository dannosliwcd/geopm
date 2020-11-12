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

#ifndef SSTSIGNAL_HPP_INCLUDE
#define SSTSIGNAL_HPP_INCLUDE

#include <memory>

#include "Signal.hpp"

namespace geopm
{
    class SSTIO;

    // TODO: think about whether to use same class with multiple constructors
    // for both MMIO and Mailbox, or different signal types
    class SSTSignal : public geopm::Signal
    {
        public:
            // signal that does not need a subcommand arg
            SSTSignal(std::shared_ptr<geopm::SSTIO> sstio,
                      bool is_mmio,
                      int cpu_idx,
                      uint16_t command,
                      uint16_t subcommand,
                      uint32_t subcommand_arg,
                      uint32_t interface_parameter);

            // // signal that requires dynamic subcommand arg value
            // SSTSignal(std::shared_ptr<geopm::SSTIO> sstio,
            //           std::shared_ptr<geopm::Signal> subcommand_arg,
            //           uint32_t command,
            //           uint32_t subcommand,
            //           int start_bit,
            //           int end_bit);

            virtual ~SSTSignal() = default;

            void setup_batch(void) override;
            double sample(void) override;
            double read(void) const override;

        private:
            std::shared_ptr<geopm::SSTIO> m_sstio;
            bool m_is_mmio;
            const int m_cpu_idx;
            const uint16_t m_command;
            const uint16_t m_subcommand;
            const uint32_t m_subcommand_arg;
            const uint32_t m_interface_parameter;

            int m_batch_idx;
    };

}

#endif
