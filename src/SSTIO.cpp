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

#include "SSTIO.hpp"

#include <fcntl.h>
#include <sys/ioctl.h>

#include "Exception.hpp"
#include "SSTIOImp.hpp"

#define GEOPM_IOC_SST_MMIO _IOWR(0xfe, 2, struct geopm::SSTIOImp::sst_mmio_interface_batch_s *)
#define GEOPM_IOC_SST_MBOX _IOWR(0xfe, 3, struct geopm::SSTIOImp::sst_mbox_interface_batch_s *)

namespace geopm
{
    std::shared_ptr<SSTIO> SSTIO::make_shared(void)
    {
        return std::make_shared<SSTIOImp>();
    }

    SSTIOImp::SSTIOImp()
        : m_path("/dev/isst_interface")
        , m_fd(open(m_path.c_str(), O_RDWR))
        , m_mbox_read_batch(nullptr)
        , m_mbox_write_batch(nullptr)
    {
        // TODO: error checking
        if (m_fd < 0) {
            throw Exception("SSTIOImp: failed to open SST driver",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);

        }
    }

    int SSTIOImp::add_mbox_read(uint32_t cpu_index, uint16_t command,
                                uint16_t subcommand, uint32_t subcommand_arg,
                                uint32_t interface_parameter)
    {
        // save the stuff in the list
        struct sst_mbox_interface_s mbox {
            .cpu_index = cpu_index,
            .mbox_interface_param = interface_parameter,
            .write_value = subcommand_arg,
            .read_value = 0,
            .command = command,
            .subcommand = subcommand,
            .reserved = 0
        };
        int idx = m_mbox_interfaces.size();
        m_mbox_interfaces.push_back(mbox);
        return idx;
    }

    int SSTIOImp::add_mbox_write(uint32_t cpu_index, uint16_t command,
                                 uint16_t subcommand, uint32_t interface_parameter,
                                 uint32_t write_value)
    {
        struct sst_mbox_interface_s mbox {
            .cpu_index = cpu_index,
            .mbox_interface_param = interface_parameter,
            .write_value = subcommand,
            .read_value = 0,
            .command = command,
            .subcommand = subcommand,
            .reserved = 0
        };
        int idx = m_mbox_interfaces.size();
        m_mbox_interfaces.push_back(mbox);
        return idx;
    }

    // call ioctl() for both mbox list and mmio list,
    // unless we end up splitting this class
    void SSTIOImp::read_batch(void)
    {
        if (!m_mbox_interfaces.empty()) {
            m_mbox_read_batch = ioctl_struct_from_vector<sst_mbox_interface_batch_s>(
                m_mbox_interfaces);

            int err = ioctl(m_fd, GEOPM_IOC_SST_MBOX, m_mbox_read_batch.get());
            if (err == -1) {
                throw Exception("SSTIOImp::read_batch(): read failed",
                                GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
        }
    }

    // TODO: might need separate call for mbox and mmio
    uint64_t SSTIOImp::sample(int batch_idx) const
    {
        return m_mbox_read_batch->interfaces[batch_idx].read_value;
    }

    void SSTIOImp::write_batch(void)
    {
        if (!m_mbox_interfaces.empty()) {
            m_mbox_write_batch = ioctl_struct_from_vector<sst_mbox_interface_batch_s>(
                m_mbox_interfaces);

            int err = ioctl(m_fd, GEOPM_IOC_SST_MBOX, m_mbox_write_batch.get());
            if (err == -1) {
                throw Exception("SSTIOImp::write_batch(): write failed",
                                GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
        }
    }

    void SSTIOImp::adjust(int index, uint64_t write_value)
    {
        // TODO: check index in range
        m_mbox_interfaces[index].write_value = write_value;
    }
}
