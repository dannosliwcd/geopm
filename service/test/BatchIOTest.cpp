/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.h"

#include "BatchIO.hpp"

#include "geopm_test.hpp"

#include "gtest/gtest.h"

using geopm::BatchIO;

class BatchIOTest : public ::testing::Test
{
    protected:
        void SetUp(void);
};

void BatchIOTest::SetUp(void)
{
    // TODO: configure a mock io_uring wrapper?
}

TEST_F(BatchIOTest, pushed_buffers_have_requested_size)
{
    //auto io = BatchIO::make_unique(/* TODO: liburing wrapper? */);
    //auto read_buffer = io->push_pread("/dev/null", 10, 0);
    //auto write_buffer = io->push_pwrite("/dev/null", 12, 0);

    //EXPECT_EQ(10, read_buffer->size());
    //EXPECT_EQ(12, write_buffer->size());
}

TEST_F(BatchIOTest, falls_back_to_individual_syscalls_if_no_io_uring)
{
    // TODO: test that make_unique gives us a non-io-uring-backed implementation
    // if IO uring cannot be used. Need to mock out io_uring_opcode_supported()
    // for this test.
    //FAIL() << "Test not implemented";
}
