/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "IOUringFallback.hpp"

#include <unistd.h>

#include "geopm/Helper.hpp"

#include <utility>

namespace geopm
{
    IOUringFallback::IOUringFallback(unsigned entries)
        : m_operations()
    {
        m_operations.reserve(entries);
    }

    IOUringFallback::~IOUringFallback()
    {
    }

    void IOUringFallback::submit()
    {
        for (const auto& operation : m_operations) {
            errno = 0;
            int ret = operation.second();
            if (ret < 0) {
                ret = -errno;
            }

            auto result_destination = operation.first;
            if (result_destination) {
                // The caller of prep_...() for this operation wants to
                // know the return value of the operation, so write it back.
                *result_destination = ret;
            }
        }

        m_operations.clear();
    }

    void IOUringFallback::register_buffers(const std::vector<iovec> &buffers_to_register)
    {
        static_cast<void>(buffers_to_register); // Has no effect in this implementation
    }

    void IOUringFallback::register_files(const std::vector<int> &files_to_register)
    {
        static_cast<void>(files_to_register); // Has no effect in this implementation
    }

    void IOUringFallback::prep_read_fixed(std::shared_ptr<int> ret, int fd, void *buf,
                                     unsigned nbytes, off_t offset, int buf_index)
    {
        static_cast<void>(buf_index); // Has no effect in this implementation
        prep_read(ret, fd, buf, nbytes, offset);
    }

    void IOUringFallback::prep_write_fixed(std::shared_ptr<int> ret, int fd, const void *buf,
                                      unsigned nbytes, off_t offset, int buf_index)
    {
        static_cast<void>(buf_index); // Has no effect in this implementation
        prep_write(ret, fd, buf, nbytes, offset);
    }

    void IOUringFallback::prep_read(std::shared_ptr<int> ret, int fd, void *buf,
                               unsigned nbytes, off_t offset)
    {
        m_operations.emplace_back(ret, std::bind(pread, fd, buf, nbytes, offset));
    }

    void IOUringFallback::prep_write(std::shared_ptr<int> ret, int fd, const void *buf,
                                unsigned nbytes, off_t offset)
    {
        m_operations.emplace_back(ret, std::bind(pwrite, fd, buf, nbytes, offset));
    }

    std::shared_ptr<IOUring> make_io_uring_fallback(unsigned entries)
    {
        return std::make_shared<IOUringFallback>(entries);
    }
}
