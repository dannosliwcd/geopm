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
#include <iostream>

#define GEOPM_IOC_SST_MMIO _IOW(0xfe, 2, struct geopm::SSTIOImp::sst_mmio_interface_batch_s *)
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
        , m_mbox_interfaces()
        , m_mmio_interfaces()
        , m_added_interfaces()
        , m_mbox_read_batch(nullptr)
        , m_mbox_write_batch(nullptr)
        , m_mmio_read_batch(nullptr)
        , m_mmio_write_batch(nullptr)
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
        int mbox_idx = m_mbox_interfaces.size();
        m_mbox_interfaces.push_back(mbox);

        int idx = m_added_interfaces.size();
        m_added_interfaces.emplace_back(false, mbox_idx);
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
        int mbox_idx = m_mbox_interfaces.size();
        m_mbox_interfaces.push_back(mbox);

        int idx = m_added_interfaces.size();
        m_added_interfaces.emplace_back(false, mbox_idx);
        return idx;
    }

    int SSTIOImp::add_mmio_read(uint32_t cpu_index, uint16_t register_offset,
                              uint32_t register_value)
    {
        struct sst_mmio_interface_s mmio {
            .is_write = 0,
            .cpu_index = cpu_index,
            .register_offset = register_offset,
            .value = register_value,
        };
        int mmio_idx = m_mmio_interfaces.size();
        m_mmio_interfaces.push_back(mmio);

        int idx = m_added_interfaces.size();
        m_added_interfaces.emplace_back(true, mmio_idx);
        return idx;
    }

    int SSTIOImp::add_mmio_write(uint32_t cpu_index, uint16_t register_offset,
                                 uint32_t register_value)
    {
        struct sst_mmio_interface_s mmio {
            .is_write = 1,
            .cpu_index = cpu_index,
            .register_offset = register_offset,
            .value = register_value,
        };
        int mmio_idx = m_mmio_interfaces.size();
        m_mmio_interfaces.push_back(mmio);

        int idx = m_added_interfaces.size();
        m_added_interfaces.emplace_back(true, mmio_idx);
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
                throw Exception("SSTIOImp::read_batch(): mbox read failed",
                                GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
        }
        if (!m_mmio_interfaces.empty()) {
            m_mmio_read_batch = ioctl_struct_from_vector<sst_mmio_interface_batch_s>(
                m_mmio_interfaces);


            std::cout << "IOCTL mmio read: "
                << m_mmio_read_batch->num_entries << " entries:";
            for (size_t i = 0; i < m_mmio_read_batch->num_entries; ++i)
            {
                std::cout
                    << "\n  cpu_index: " << m_mmio_read_batch->interfaces[i].cpu_index
                    << "\n  is_write: " << m_mmio_read_batch->interfaces[i].is_write
                    << "\n  register_offset: " << m_mmio_read_batch->interfaces[i].register_offset
                    << "\n  value: " << m_mmio_read_batch->interfaces[i].value;
            }
            std::cout << std::endl;
            int err = ioctl(m_fd, GEOPM_IOC_SST_MMIO, m_mmio_read_batch.get());
            if (err == -1) {
                throw Exception("SSTIOImp::read_batch(): mmio read failed",
                                GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
        }
    }

    uint64_t SSTIOImp::sample(int batch_idx) const
    {
        const auto& interface = m_added_interfaces[batch_idx];
        uint64_t sample_value;
        if (interface.first) {
            sample_value = m_mmio_read_batch->interfaces[interface.second].value;
        }
        else {
            sample_value = m_mbox_read_batch->interfaces[interface.second].read_value;
        }
        return sample_value;
    }

    void SSTIOImp::write_batch(void)
    {
        if (!m_mbox_interfaces.empty()) {
            m_mbox_write_batch = ioctl_struct_from_vector<sst_mbox_interface_batch_s>(
                m_mbox_interfaces);
            std::cout << "IOCTL mbox write: "
                << m_mbox_write_batch->num_entries << " entries:";
            std::cout << std::endl;

            int err = ioctl(m_fd, GEOPM_IOC_SST_MBOX, m_mbox_write_batch.get());
            if (err == -1) {
                throw Exception("sstioimp::write_batch(): write failed",
                                GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
        }
        if (!m_mmio_interfaces.empty()) {
            m_mmio_write_batch = ioctl_struct_from_vector<sst_mmio_interface_batch_s>(
                m_mmio_interfaces);

            std::cout << "IOCTL mmio write: "
                << m_mmio_write_batch->num_entries << " entries:";
            for (size_t i = 0; i < m_mmio_write_batch->num_entries; ++i)
            {
                std::cout
                    << "\n  cpu_index: " << m_mmio_write_batch->interfaces[i].cpu_index
                    << "\n  is_write: " << m_mmio_write_batch->interfaces[i].is_write
                    << "\n  register_offset: " << m_mmio_write_batch->interfaces[i].register_offset
                    << "\n  value: " << m_mmio_write_batch->interfaces[i].value;
            }
            std::cout << std::endl;
            int err = ioctl(m_fd, GEOPM_IOC_SST_MMIO, m_mmio_write_batch.get());
            if (err == -1) {
                throw Exception("sstioimp::write_batch(): write failed",
                                GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
        }
    }

    void SSTIOImp::adjust(int index, uint64_t write_value)
    {
        // TODO: check index in range
        const auto& interface = m_added_interfaces[index];
        if (interface.first) {
            m_mmio_interfaces[interface.second].value = write_value;
        }
        else {
            m_mbox_interfaces[interface.second].write_value = write_value;
        }
    }
}
