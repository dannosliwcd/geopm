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

#ifndef SST_HPP_INCLUDE
#define SST_HPP_INCLUDE

#include <cstdint>

#include <vector>
#include <memory>

namespace geopm
{
    class SSTIO
    {
        public:
            /// Interact with the mailbox on commands that are expected to return data
            virtual int add_mbox_read(uint32_t cpu_index, uint16_t command,
                                      uint16_t subcommand, uint32_t subcommand_arg,
                                      uint32_t interface_parameter) = 0;

            // TODO: write_value probably will go away
            /// Interact with the mailbox on commands that are not expected to return data
            virtual int add_mbox_write(uint32_t cpu_index, uint16_t command,
                                       uint16_t subcommand, uint32_t interface_parameter,
                                       uint32_t write_value) = 0;

            /// Interact with the mmio interface on commands that are expected to return data
            virtual int add_mmio_read(uint32_t cpu_index, uint16_t register_offset,
                                      uint32_t register_value) = 0;

            // TODO: write_value probably will go away
            /// Interact with the mmio interface on commands that are not expected to return data
            virtual int add_mmio_write(uint32_t cpu_index, uint16_t register_offset,
                                       uint32_t register_value) = 0;

            // /// Read data from the mmio interface
            // virtual int add_mmio_read(uint32_t cpu_index, uint32_t register_offset) = 0;

            // /// Write data to the mmio interface
            // virtual int add_mmio_write(uint32_t cpu_index, uint32_t register_offset,
            //                         uint32_t value) = 0;

            // call ioctl() for both mbox list and mmio list,
            // unless we end up splitting this class
            virtual void read_batch(void) = 0;

            // TODO: might need separate call for mbox and mmio
            virtual uint64_t sample(int index) const = 0;

            // See also the mock implementation
            virtual void write_batch(void) = 0;

            virtual void adjust(int index, uint64_t write_value) = 0;

            virtual uint32_t get_punit_from_cpu(uint32_t cpu_index) = 0;

            static std::shared_ptr<SSTIO> make_shared(int max_cpus);
    };

}

#endif
