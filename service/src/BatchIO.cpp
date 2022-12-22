/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.h"
#include "BatchIO.hpp"

namespace geopm
{
    class BatchIOImp
    {
        public:
            BatchIOImp() = default;
            virtual ~BatchIOImp() = default;
            std::shared_ptr<BatchBufferRead> push_pread(std::string file_path, size_t count, off_t offset) override;
            std::shared_ptr<BatchBufferWrite> push_pwrite(std::string file_path, size_t count, off_t offset) override;
            void read_batch(void) override;
            void write_batch(void) override;
            void reset(void) override;
        private:
            std::vector<shared_ptr::BatchBufferReadImp> m_read_map;
            std::vector<shared_ptr::BatchBufferWriteImp> m_write_map;
    };


    class BatchBufferReadImp : public BatchBufferRead
    {
        public:
            BatchBufferReadImp(const void *buffer, size_t size) = default;
            virtual ~BatchBufferReadImp() = default;
            size_t size(void) const override;
            const void *data(void) const override;
            std::string str(void) const override;
        private:
            const void *m_buffer;
            const size_t m_size;
    }

    class BatchBufferWriteImp : public BatchBufferWrite
    {
        public:
            BatchBufferWriteImp(void *buffer, size_t) = default;
            virtual ~BatchBufferWriteImp() = default;
            size_t size(void) const override;
            void *data(void) override;
            void str(const std::string &input) override;
            bool is_dirty;
        private:
            void *m_buffer;
            size_t m_size;
    }
}

#endif
