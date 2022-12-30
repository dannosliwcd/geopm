/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "config.h"

#include "MSRIOImp.hpp"

#include <algorithm>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sstream>
#include <map>

#include "geopm_error.h"
#include "geopm_sched.h"
#include "geopm_debug.hpp"
#include "geopm/Exception.hpp"
#include "geopm/Helper.hpp"
#include "IOUring.hpp"
#include "MSRPath.hpp"

#define GEOPM_IOC_MSR_BATCH _IOWR('c', 0xA2, struct geopm::MSRIOImp::m_msr_batch_array_s)

namespace geopm
{
    std::unique_ptr<MSRIO> MSRIO::make_unique(void)
    {
        return geopm::make_unique<MSRIOImp>();
    }

    std::shared_ptr<MSRIO> MSRIO::make_shared(void)
    {
        return std::make_shared<MSRIOImp>();
    }

    MSRIOImp::MSRIOImp()
        : MSRIOImp(geopm_sched_num_cpu(), std::make_shared<MSRPath>(), make_io_uring)
    {

    }

    MSRIOImp::MSRIOImp(int num_cpu, std::shared_ptr<MSRPath> path,
                       IOUringFactory batch_io_factory)
        : m_num_cpu(num_cpu)
        , m_file_desc(m_num_cpu + 1, -1) // Last file descriptor is for the batch file
        , m_is_batch_enabled(false) // TODO remove later. forcing read/write for testing
        , m_read_batch({0, NULL})
        , m_write_batch({0, NULL})
        , m_read_batch_op(0)
        , m_write_batch_op(0)
        , m_is_batch_read(false)
        , m_read_batch_idx_map(m_num_cpu)
        , m_write_batch_idx_map(m_num_cpu)
        , m_is_open(false)
        , m_path(path)
        , m_batch_io_factory(batch_io_factory)
        , m_batch_reader(nullptr)
        , m_batch_writer(nullptr)
    {
        open_all();
    }

    MSRIOImp::~MSRIOImp()
    {
        close_all();
    }

    void MSRIOImp::open_all(void)
    {
        if (!m_is_open) {
            for (int cpu_idx = 0; cpu_idx < m_num_cpu; ++cpu_idx) {
                open_msr(cpu_idx);
            }
            open_msr_batch();
            m_is_open = true;
        }
    }

    void MSRIOImp::close_all(void)
    {
        if (m_is_open) {
            close_msr_batch();
            for (int cpu_idx = m_num_cpu - 1; cpu_idx != -1; --cpu_idx) {
                close_msr(cpu_idx);
            }
            m_is_open = false;
        }
    }

    uint64_t MSRIOImp::read_msr(int cpu_idx,
                                uint64_t offset)
    {
        uint64_t result = 0;
        size_t num_read = pread(msr_desc(cpu_idx), &result, sizeof(result), offset);
        if (num_read != sizeof(result)) {
            std::ostringstream err_str;
            err_str << "MSRIOImp::read_msr(): pread() failed at offset 0x" << std::hex << offset
                    << " system error: " << strerror(errno);
            throw Exception(err_str.str(), GEOPM_ERROR_MSR_READ, __FILE__, __LINE__);
        }
        return result;
    }

    void MSRIOImp::write_msr(int cpu_idx,
                             uint64_t offset,
                             uint64_t raw_value,
                             uint64_t write_mask)
    {
        if ((raw_value & write_mask) != raw_value) {
            std::ostringstream err_str;
            err_str << "MSRIOImp::write_msr(): raw_value does not obey write_mask, "
                    << "raw_value=0x" << std::hex << raw_value
                    << " write_mask=0x" << write_mask;
            throw Exception(err_str.str(), GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        uint64_t write_value = read_msr(cpu_idx, offset);
        write_value &= ~write_mask;
        write_value |= raw_value;
        size_t num_write = pwrite(msr_desc(cpu_idx), &write_value, sizeof(write_value), offset);
        if (num_write != sizeof(write_value)) {
            std::ostringstream err_str;
            err_str << "MSRIOImp::write_msr(): pwrite() failed at offset 0x" << std::hex << offset
                    << " system error: " << strerror(errno);
            throw Exception(err_str.str(), GEOPM_ERROR_MSR_WRITE, __FILE__, __LINE__);
        }
    }

    uint64_t MSRIOImp::system_write_mask(uint64_t offset)
    {
        if (!m_is_batch_enabled) {
            return ~0ULL;
        }
        uint64_t result = 0;
        auto off_it = m_offset_mask_map.find(offset);
        if (off_it != m_offset_mask_map.end()) {
            result = off_it->second;
        }
        else {
            m_msr_batch_op_s wr {
                .cpu = 0,
                .isrdmsr = 1,
                .err = 0,
                .msr = (uint32_t)offset,
                .msrdata = 0,
                .wmask = 0,
            };
            m_msr_batch_array_s arr {
                .numops = 1,
                .ops = &wr,
            };
            int err = ioctl(msr_batch_desc(), GEOPM_IOC_MSR_BATCH, &arr);
            if (err || wr.err) {
                throw Exception("MSRIOImp::system_write_mask(): read of mask failed",
                                GEOPM_ERROR_INVALID, __FILE__, __LINE__);
            }
            m_offset_mask_map[offset] = wr.wmask;
            result = wr.wmask;
        }
        return result;
   }

    int MSRIOImp::add_write(int cpu_idx, uint64_t offset)
    {
        int result = -1;
        auto batch_it = m_write_batch_idx_map.at(cpu_idx).find(offset);
        if (batch_it == m_write_batch_idx_map[cpu_idx].end()) {
            result = m_write_batch_op.size();
            m_msr_batch_op_s wr {
                .cpu = (uint16_t)cpu_idx,
                .isrdmsr = 1,
                .err = 0,
                .msr = (uint32_t)offset,
                .msrdata = 0,
                .wmask = system_write_mask(offset),
            };
            m_write_batch_op.push_back(wr);
            m_write_val.push_back(0);
            m_write_mask.push_back(0);  // will be widened to match writes by adjust()
            m_write_batch_idx_map[cpu_idx][offset] = result;
        }
        else {
            result = batch_it->second;
        }
        return result;
    }

    void MSRIOImp::adjust(int batch_idx, uint64_t raw_value, uint64_t write_mask)
    {
        if (batch_idx < 0 || (size_t)batch_idx >= m_write_batch_op.size()) {
            throw Exception("MSRIOImp::adjust(): batch_idx out of range: " + std::to_string(batch_idx),
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        GEOPM_DEBUG_ASSERT(m_write_batch_op.size() == m_write_val.size() &&
                           m_write_batch_op.size() == m_write_mask.size(),
                           "Size of member vectors does not match");
        uint64_t wmask_sys = m_write_batch_op[batch_idx].wmask;
        if ((~wmask_sys & write_mask) != 0ULL) {
            throw Exception("MSRIOImp::adjust(): write_mask is out of bounds",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        if ((raw_value & write_mask) != raw_value) {
            std::ostringstream err_str;
            err_str << "MSRIOImp::adjust(): raw_value does not obey write_mask, "
                    << "raw_value=0x" << std::hex << raw_value
                    << " write_mask=0x" << write_mask;
            throw Exception(err_str.str(), GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        m_write_val[batch_idx] &= ~write_mask;
        m_write_val[batch_idx] |= raw_value;
        m_write_mask[batch_idx] |= write_mask;
    }

    int MSRIOImp::add_read(int cpu_idx, uint64_t offset)
    {
        /// @todo return same index for repeated calls with same inputs.
        m_msr_batch_op_s rd {
            .cpu = (uint16_t)cpu_idx,
            .isrdmsr = 1,
            .err = 0,
            .msr = (uint32_t)offset,
            .msrdata = 0,
            .wmask = 0
        };
        int idx = m_read_batch_op.size();
        m_read_batch_op.push_back(rd);
        return idx;
    }

    uint64_t MSRIOImp::sample(int batch_idx) const
    {
        if (!m_is_batch_read) {
            throw Exception("MSRIOImp::sample(): cannot call sample() before read_batch().",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        return m_read_batch.ops[batch_idx].msrdata;
    }


    void MSRIOImp::msr_ioctl(struct m_msr_batch_array_s &batch)
    {
        int err = ioctl(msr_batch_desc(), GEOPM_IOC_MSR_BATCH, &batch);
        if (err) {
            err = errno ? errno : err;
            std::ostringstream err_str;
            err_str << "MSRIOImp::msr_ioctl(): call to ioctl() for /dev/cpu/msr_batch failed: "
                    << " system error: " << strerror(err);
            throw Exception(err_str.str(), GEOPM_ERROR_MSR_READ, __FILE__, __LINE__);
        }
        for (uint32_t batch_idx = 0; batch_idx != batch.numops; ++batch_idx) {
            err = batch.ops[batch_idx].err;
            if (err) {
                auto offset = batch.ops[batch_idx].msr;
                std::ostringstream err_str;
                err_str << "MSRIOImp::msr_ioctl(): operation failed at offset 0x"
                        << std::hex << offset
                        << " system error: " << strerror(err);
                throw Exception(err_str.str(), GEOPM_ERROR_MSR_WRITE, __FILE__, __LINE__);
            }
        }
    }

   void MSRIOImp::msr_ioctl_read(void)
    {
        if (m_read_batch.numops == 0) {
            return;
        }
        GEOPM_DEBUG_ASSERT(m_read_batch.numops == m_read_batch_op.size() &&
                           m_read_batch.ops == m_read_batch_op.data(),
                           "Batch operations not updated prior to calling MSRIOImp::msr_ioctl_read()");
        msr_ioctl(m_read_batch);
    }

    void MSRIOImp::msr_ioctl_write(void)
    {
        if (m_write_batch.numops == 0) {
            return;
        }
        GEOPM_DEBUG_ASSERT(m_write_batch.numops == m_write_batch_op.size() &&
                           m_write_batch.ops == m_write_batch_op.data(),
                           "MSRIOImp::msr_ioctl_write(): Batch operations not updated prior to calling");
        if (m_write_val.size() != m_write_batch.numops ||
            m_write_mask.size() != m_write_batch.numops ||
            m_write_batch_op.size() != m_write_batch.numops) {
            throw Exception("MSRIOImp::msr_ioctl_write(): Invalid operations stored in object, incorrectly sized",
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        msr_ioctl(m_write_batch);
        // Modify with write mask
        int op_idx = 0;
        for (auto &op_it : m_write_batch_op) {
            op_it.isrdmsr = 0;
            op_it.msrdata &= ~m_write_mask[op_idx];
            op_it.msrdata |= m_write_val[op_idx];
            GEOPM_DEBUG_ASSERT((~op_it.wmask & m_write_mask[op_idx]) == 0ULL,
                               "MSRIOImp::msr_ioctl_write(): Write mask violation at write time");
            ++op_idx;
        }
        msr_ioctl(m_write_batch);
        for (auto &op_it : m_write_batch_op) {
            op_it.isrdmsr = 1;
        }
    }

    void MSRIOImp::msr_read_files(void)
    {
        if (m_read_batch.numops == 0) {
            return;
        }
        GEOPM_DEBUG_ASSERT(m_read_batch.numops == m_read_batch_op.size() &&
                           m_read_batch.ops == m_read_batch_op.data(),
                           "Batch operations not updated prior to calling "
                           "MSRIOImp::msr_read_files()");

        if (!m_batch_reader) {
            m_batch_reader = m_batch_io_factory(m_read_batch.numops);
            std::vector<iovec> iov;
            std::vector<int> fds;
            for (uint32_t batch_idx = 0; batch_idx != m_read_batch.numops; ++batch_idx) {
                auto& batch_op = m_read_batch.ops[batch_idx];
                iov.push_back({.iov_base = &batch_op.msrdata,
                               .iov_len = sizeof(batch_op.msrdata) });
                fds.push_back(msr_desc(batch_op.cpu));
            }
            // TODO: This results in one registered buffer per signal. Each
            // registered buffer gets a page locked into memory. We don't need
            // a whole page (we only need 8 bytes for each buffer), so this
            // unnecessarily eats into our process's max locked memory size.
            //
            // Initial measurements don't show significant latency improvements
            // from using registered buffers in this IOGroup (tested on a
            // 386-signal batch-read loop).
            m_batch_reader->register_buffers(iov);
            m_batch_reader->register_files(fds);
        }
        msr_batch_io(*m_batch_reader, m_read_batch);
    }

    void MSRIOImp::msr_batch_io(IOUring &batcher,
                                struct m_msr_batch_array_s &batch)
    {
        std::vector<std::shared_ptr<int> > return_values;
        return_values.reserve(batch.numops);

        for (uint32_t batch_idx = 0; batch_idx != batch.numops; ++batch_idx) {
            return_values.emplace_back(new int(0));
            auto& batch_op = batch.ops[batch_idx];
            if (batch_op.isrdmsr) {
                batcher.prep_read_fixed(return_values.back(), msr_desc(batch_op.cpu),
                                        &batch_op.msrdata, sizeof(batch_op.msrdata),
                                        batch_op.msr, batch_idx);
            }
            else {
                batcher.prep_write_fixed(return_values.back(), msr_desc(batch_op.cpu),
                                         &batch_op.msrdata, sizeof(batch_op.msrdata),
                                         batch_op.msr, batch_idx);
            }
        }

        batcher.submit();

        for (uint32_t batch_idx = 0; batch_idx != batch.numops; ++batch_idx) {
            ssize_t successful_bytes = *return_values[batch_idx];
            auto& batch_op = batch.ops[batch_idx];
            if (successful_bytes != sizeof(batch_op.msrdata)) {
                std::ostringstream err_str;
                err_str << "MSRIOImp::msr_batch_io(): failed at offset 0x"
                        << std::hex << batch_op.msr
                        << " system error: "
                        << ((successful_bytes < 0) ? strerror(-successful_bytes) : "none");
                throw Exception(err_str.str(), batch_op.isrdmsr
                                ? GEOPM_ERROR_MSR_READ : GEOPM_ERROR_MSR_WRITE,
                                __FILE__, __LINE__);
            }
        }
    }

    void MSRIOImp::msr_rmw_files(void)
    {
        if (m_write_batch.numops == 0) {
            return;
        }
        GEOPM_DEBUG_ASSERT(m_write_batch.numops == m_write_batch_op.size() &&
                           m_write_batch.ops == m_write_batch_op.data(),
                           "Batch operations not updated prior to calling "
                           "MSRIOImp::msr_rmw_files()");

        if (!m_batch_writer) {
            m_batch_writer = m_batch_io_factory(m_write_batch.numops);
            std::vector<iovec> iov;
            for (uint32_t batch_idx = 0; batch_idx != m_write_batch.numops; ++batch_idx) {
                auto& batch_op = m_write_batch.ops[batch_idx];
                iov.push_back({.iov_base = &batch_op.msrdata,
                               .iov_len = sizeof(batch_op.msrdata) });
            }
            m_batch_writer->register_buffers(iov);
        }

        // Read existing MSR values
        msr_batch_io(*m_batch_writer, m_write_batch);

        // Modify with write mask
        int op_idx = 0;
        for (auto &op_it : m_write_batch_op) {
            op_it.isrdmsr = 0;
            op_it.msrdata &= ~m_write_mask[op_idx];
            op_it.msrdata |= m_write_val[op_idx];
            GEOPM_DEBUG_ASSERT((~op_it.wmask & m_write_mask[op_idx]) == 0ULL,
                               "MSRIOImp::msr_rmw_files(): Write mask "
                               "violation at write time");
            ++op_idx;
        }

        // Write back the modified MSRs
        msr_batch_io(*m_batch_writer, m_write_batch);
        for (auto &op_it : m_write_batch_op) {
            op_it.isrdmsr = 1;
        }
    }

    void MSRIOImp::read_batch(void)
    {
        m_read_batch.numops = m_read_batch_op.size();
        m_read_batch.ops = m_read_batch_op.data();

        // Use the batch-oriented MSR-safe ioctl if possible. Otherwise, operate
        // over individual read operations per MSR.
        if (m_is_batch_enabled) {
            msr_ioctl_read();
        }
        else {
            msr_read_files();
        }
        m_is_batch_read = true;
    }

    void MSRIOImp::write_batch(void)
    {
        m_write_batch.numops = m_write_batch_op.size();
        m_write_batch.ops = m_write_batch_op.data();

        // Use the batch-oriented MSR-safe ioctl twice (batch-read, modify,
        // batch-write) if possible. Otherwise, operate over individual
        // read-modify-write operations per MSR.
        if (m_is_batch_enabled) {
            msr_ioctl_write();
        }
        else {
            msr_rmw_files();
        }
        std::fill(m_write_val.begin(), m_write_val.end(), 0ULL);
        std::fill(m_write_mask.begin(), m_write_mask.end(), 0ULL);
        m_is_batch_read = true;
    }

    int MSRIOImp::msr_desc(int cpu_idx)
    {
        if (cpu_idx < 0 || cpu_idx > m_num_cpu) {
            throw Exception("MSRIOImp::msr_desc(): cpu_idx=" + std::to_string(cpu_idx) +
                            " out of range, num_cpu=" + std::to_string(m_num_cpu),
                            GEOPM_ERROR_INVALID, __FILE__, __LINE__);
        }
        return m_file_desc[cpu_idx];
    }

    int MSRIOImp::msr_batch_desc()
    {
        return m_file_desc[m_num_cpu];
    }

    void MSRIOImp::open_msr(int cpu_idx)
    {
        for (int fallback_idx = 0;
             m_file_desc[cpu_idx] == -1;
             ++fallback_idx) {
            std::string path = m_path->msr_path(cpu_idx, fallback_idx);
            m_file_desc[cpu_idx] = open(path.c_str(), O_RDWR);
        }
        struct stat stat_buffer;
        int err = fstat(m_file_desc[cpu_idx], &stat_buffer);
        if (err) {
            throw Exception("MSRIOImp::open_msr(): file descriptor invalid",
                            GEOPM_ERROR_MSR_OPEN, __FILE__, __LINE__);
        }
    }

    void MSRIOImp::open_msr_batch(void)
    {
        if (m_is_batch_enabled && m_file_desc[m_num_cpu] == -1) {
            std::string path = m_path->msr_batch_path();
            m_file_desc[m_num_cpu] = open(path.c_str(), O_RDWR);
            if (m_file_desc[m_num_cpu] == -1) {
                m_is_batch_enabled = false;
            }
        }
        if (m_is_batch_enabled) {
            struct stat stat_buffer;
            int err = fstat(m_file_desc[m_num_cpu], &stat_buffer);
            if (err) {
                throw Exception("MSRIOImp::open_msr_batch(): file descriptor invalid",
                                GEOPM_ERROR_MSR_OPEN, __FILE__, __LINE__);
            }
        }
    }

    void MSRIOImp::close_msr(int cpu_idx)
    {
        if (m_file_desc[cpu_idx] != -1) {
            (void)close(m_file_desc[cpu_idx]);
            m_file_desc[cpu_idx] = -1;
        }
    }

    void MSRIOImp::close_msr_batch(void)
    {
        if (m_file_desc[m_num_cpu] != -1) {
            (void)close(m_file_desc[m_num_cpu]);
            m_file_desc[m_num_cpu] = -1;
        }
    }
}
