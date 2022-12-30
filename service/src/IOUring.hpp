/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOURING_HPP_INCLUDE
#define IOURING_HPP_INCLUDE

#include <cstdint>
#include <memory>
#include <functional>
#include <vector>
#include <sys/uio.h>

namespace geopm
{
    class IOUring
    {
        public:
            /// @brief Create and initialize an IO uring.
            IOUring() = default;

            /// @brief Clean up the IO uring.
            virtual ~IOUring() = default;

            /// @brief Submit all prepared uring operations in a batch, and
            ///        wait for all operations to return a result. Throws if
            ///        there are errors interacting with the completion queue.
            ///        Failures of batched operations are reported by @p ret
            ///        from the operation's respective prep_...  function call,
            ///        and do not cause this function to throw.
            virtual void submit() = 0;

            /// @brief Register buffers for use with prep_read_fixed and
            /// prep_write_fixed.
            /// @param buffers_to_register iovecs indicating which buffers to
            ///        register. Order matters, since the order specified here maps
            ///        @p buf_index to buffers for prep_..._fixed functions.
            virtual void register_buffers(const std::vector<iovec> &buffers_to_register) = 0;

            /// @brief Register files for use by vector offset instead of using
            ///        file descriptors in read/write functions.
            /// @param files_to_register File descriptors indicating which files to
            ///        register. Order matters, since the order specified here maps
            ///        @p fd as an index to files for prep_..._fixed functions.
            virtual void register_files(const std::vector<int> &files_to_register) = 0;

            virtual void prep_read_fixed(std::shared_ptr<int> ret, int fd,
                                 void *buf, unsigned nbytes, off_t offset,
                                 int buf_index) = 0;

            virtual void prep_write_fixed(std::shared_ptr<int> ret, int fd,
                                  const void *buf, unsigned nbytes, off_t offset,
                                  int buf_index) = 0;

            /// @brief Perform a pread in the next batch submission.
            /// @param ret  Where to store the operation's return value,
            ///             which will be a non-negative number of bytes read,
            ///             or -errno on failure, after submit() returns.
            /// @param fd  Which already-opened file to read.
            /// @param buf  Where to store the read data.
            /// @param nbytes  Number of bytes to read into @p buf.
            /// @param offset  Offset within fd to start the pread. -1 uses the
            ///                existing offset of @p fd, like in read().
            /// @throw if the queue is already full.
            virtual void prep_read(std::shared_ptr<int> ret, int fd,
                                   void *buf, unsigned nbytes, off_t offset) = 0;

            /// @brief Perform a pwrite in the next batch submission.
            /// @param ret  Where to store the operation's return value,
            ///             which will be a non-negative number of bytes written,
            ///             or -errno on failure, after submit() returns.
            /// @param fd  Which already-opened file to write.
            /// @param buf  Which data to write to the file.
            /// @param nbytes  Number of bytes to write from @p buf.
            /// @param offset  Offset within fd to start the pwrite. -1 uses the
            ///                existing offset of @p fd, like in write().
            /// @throw if the queue is already full.
            virtual void prep_write(std::shared_ptr<int> ret, int fd,
                                    const void *buf, unsigned nbytes, off_t offset) = 0;
    };

    /// @brief Create an object that supports an io_uring-like interface. The
    ///        created object uses io_uring if supported, otherwise uses
    ///        individual read/write operations.
    /// @param entries Maximum expected batch size
    std::shared_ptr<IOUring> make_io_uring(unsigned entries);
    using IOUringFactory = std::function<decltype(make_io_uring)>;
}

#endif /* IOURING_HPP_INCLUDE */
