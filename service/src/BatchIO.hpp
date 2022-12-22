/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BATCHIO_HPP_INCLUDE
#define BATCHIO_HPP_INCLUDE

#include <string>
#include <memory>

namespace geopm
{
    class BatchIO
    {
        public:
            BatchIO() = default;
            virtual ~BatchIO() = default;
            static std::unique_ptr<BatchIO> make_unique();
            virtual std::shared_ptr<BatchBufferRead> push_pread(std::string file_path, size_t count, off_t offset) = 0;
            virtual std::shared_ptr<BatchBufferWrite> push_pwrite(std::string file_path, size_t count, off_t offset) = 0;
            virtual void read_batch(void) = 0;
            virtual void write_batch(void) = 0;
            virtual void reset(void) = 0;
    };

    class BatchBufferRead
    {
        public:
            BatchBufferRead() = default;
            virtual ~BatchBufferRead() = default;
            virtual size_t size(void) const = 0;
            virtual const void *data(void) const = 0;
            virtual std::string str(void) const = 0;
    }

    class BatchBufferWrite
    {
        public:
            BatchBufferWrite() = default;
            virtual ~BatchBufferWrite() = default;
            virtual size_t size(void) const = 0;
            virtual void *data(void) = 0;
            virtual void str(const std::string &input) = 0;
    }
}

#endif
