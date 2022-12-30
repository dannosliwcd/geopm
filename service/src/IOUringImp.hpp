/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOURINGIMP_HPP_INCLUDE
#define IOURINGIMP_HPP_INCLUDE

#define _POSIX_C_SOURCE 200809L
#include <liburing.h>

#include "IOUring.hpp"

#include <vector>

namespace geopm
{
    /// @brief Implementation of the IOUring batch interface. This
    /// implementation batches operations inside io_uring submission queues.
    class IOUringImp final : public IOUring
    {
        public:
            IOUringImp(unsigned entries);
            virtual ~IOUringImp();

            void submit() override;

            void prep_read(std::shared_ptr<int> ret, int fd,
                           void *buf, unsigned nbytes, off_t offset) override;

            void prep_write(std::shared_ptr<int> ret, int fd,
                            const void *buf, unsigned nbytes, off_t offset) override;

            /// @brief Return whether this implementation of IOUring is supported.
            static bool is_supported();
        protected:
            struct io_uring_sqe *get_sqe_or_throw();
            void set_sqe_return_destination(
                    struct io_uring_sqe* sqe,
                    std::shared_ptr<int> destination);
        private:
            struct io_uring m_ring;
            std::vector<std::shared_ptr<int> > m_result_destinations;
    };

    /// @brief Create an IO uring with queues of a given size.
    /// @param entries  Maximum number of queue operations to contain
    ///                 within a single batch submission.
    std::shared_ptr<IOUring> make_io_uring_imp(unsigned entries);
}
#endif // IOURINGIMP_HPP_INCLUDE
