/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOURINGFALLBACK_HPP_INCLUDE
#define IOURINGFALLBACK_HPP_INCLUDE

#include "IOUring.hpp"
#include <functional>
#include <vector>

namespace geopm
{
    class IOUringFallback final : public IOUring
    {
        public:
            IOUringFallback(unsigned entries);
            virtual ~IOUringFallback();

            void submit() override;

            void prep_read(std::shared_ptr<int> ret, int fd,
                           void *buf, unsigned nbytes, off_t offset) override;

            void prep_write(std::shared_ptr<int> ret, int fd,
                            const void *buf, unsigned nbytes, off_t offset) override;
        private:
            // Pairs of pointers where operation results are desired, and functions
            // that perform the operation and forward its return value.
            using FutureOperation = std::pair<std::shared_ptr<int>, std::function<int()> >;
            std::vector<FutureOperation> m_operations;
    };

    /// @brief Create a fallback implementation of IOUring that uses non-batched
    ///        IO operations, in case we cannot use IO uring or liburing.
    std::unique_ptr<IOUring> make_io_uring_fallback(unsigned entries);
}
#endif // IOURINGFALLBACK_HPP_INCLUDE
