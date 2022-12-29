/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.h"

#include "BatchIO.hpp"

#include "geopm/Exception.hpp"
#include "geopm_error.h"

#include <vector>

namespace geopm
{
    class BatchBufferReadImp : public BatchBufferRead
    {
        public:
            BatchBufferReadImp(const void *buffer, size_t size);
            virtual ~BatchBufferReadImp() = default;
            size_t size(void) const override;
            const void *data(void) const override;
            std::string str(void) const override;

        private:
            const void *m_buffer;
            const size_t m_size;
    };

    class BatchBufferWriteImp : public BatchBufferWrite
    {
        public:
            BatchBufferWriteImp(void *buffer, size_t size);
            virtual ~BatchBufferWriteImp() = default;
            size_t size(void) const override;
            void *data(void) override;
            void str(const std::string &input) override;
            bool is_dirty;

        private:
            void *m_buffer;
            size_t m_size;
    };

    class BatchIOImp : public BatchIO
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
            std::vector<std::shared_ptr<BatchBufferReadImp> > m_read_map;
            std::vector<std::shared_ptr<BatchBufferWriteImp> > m_write_map;
    };

    BatchBufferReadImp::BatchBufferReadImp(const void *buffer, size_t size)
        : m_buffer(buffer)
        , m_size(size)
    {
        throw Exception("Called an unimplemented function",
                        GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    }

    size_t BatchBufferReadImp::size(void) const
    {
        return m_size;
    }

    const void *BatchBufferReadImp::data(void) const
    {
        return m_buffer;
    }

    std::string BatchBufferReadImp::str(void) const
    {
        throw Exception("Called an unimplemented function",
                        GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    }

    BatchBufferWriteImp::BatchBufferWriteImp(void *buffer, size_t size)
        : m_buffer(buffer)
        , m_size(size)
    {
        throw Exception("Called an unimplemented function",
                        GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    }

    size_t BatchBufferWriteImp::size(void) const
    {
        return m_size;
    }

    void *BatchBufferWriteImp::data(void)
    {
        // TOOD: mark dirty so we know to push in the next batch?
        return m_buffer;
    }

    void BatchBufferWriteImp::str(const std::string & /* unused: input */)
    {
        throw Exception("Called an unimplemented function",
                        GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    }

    std::shared_ptr<BatchBufferRead> BatchIOImp::push_pread(
        std::string /* unused: file_path */, size_t /* unused: count */, off_t /* unused: offset */)
    {
        throw Exception("Called an unimplemented function",
                        GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    }

    std::shared_ptr<BatchBufferWrite> BatchIOImp::push_pwrite(
        std::string /* unused: file_path */, size_t /* unused: count */, off_t /* unused: offset */)
    {
        throw Exception("Called an unimplemented function",
                        GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    }

    void BatchIOImp::read_batch(void)
    {
        throw Exception("Called an unimplemented function",
                        GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    }

    void BatchIOImp::write_batch(void)
    {
        throw Exception("Called an unimplemented function",
                        GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    }

    void BatchIOImp::reset(void)
    {
        throw Exception("Called an unimplemented function",
                        GEOPM_ERROR_LOGIC, __FILE__, __LINE__);
    }

}
