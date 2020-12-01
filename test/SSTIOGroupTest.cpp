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

#include <memory>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "Helper.hpp"
#include "SSTIOGroup.hpp"
#include "MockPlatformTopo.hpp"
#include "MockSSTIO.hpp"

using geopm::SSTIOGroup;
using testing::Return;
using testing::_;
using testing::AtLeast;

class SSTIOGroupTest : public :: testing :: Test
{
    protected:
        void SetUp();
        std::shared_ptr<MockSSTIO> m_sstio;
        std::unique_ptr<SSTIOGroup> m_group;
        std::shared_ptr<MockPlatformTopo> m_topo;
        int m_num_package = 2;
        int m_num_core = 4;
        int m_num_cpu = 16;
};

void SSTIOGroupTest::SetUp()
{
    m_topo = make_topo(m_num_package, m_num_core, m_num_cpu);
    EXPECT_CALL(*m_topo, domain_nested(_, _, _)).Times(AtLeast(0));
    EXPECT_CALL(*m_topo, num_domain(_)).Times(AtLeast(0));

    m_sstio = std::make_shared<MockSSTIO>();

    m_group = geopm::make_unique<SSTIOGroup>(*m_topo, m_sstio);
}

TEST_F(SSTIOGroupTest, valid_signal_names)
{
    auto names = m_group->signal_names();
    for (auto nn : names) {
        std::cout << nn << std::endl;
    }
}

TEST_F(SSTIOGroupTest, valid_signal_domains)
{
}

TEST_F(SSTIOGroupTest, valid_signal_aggregation)
{
}

TEST_F(SSTIOGroupTest, valid_signal_format)
{
}


TEST_F(SSTIOGroupTest, push_signal)
{
}

TEST_F(SSTIOGroupTest, sample_config_level)
{
    enum sst_idx_e {
        CONFIG_LEVEL_0,
        CONFIG_LEVEL_1
    };

    int pkg_0_cpu = 0;
    int pkg_1_cpu = 2;

    EXPECT_CALL(*m_sstio, add_mbox_read(pkg_0_cpu, 0x7F, 0x00, 0x00, 0x00))
        .WillOnce(Return(CONFIG_LEVEL_0));
    EXPECT_CALL(*m_sstio, add_mbox_read(pkg_1_cpu, 0x7F, 0x00, 0x00, 0x00))
        .WillOnce(Return(CONFIG_LEVEL_1));

    int idx0 = m_group->push_signal("SST::CONFIG_LEVEL:LEVEL", GEOPM_DOMAIN_PACKAGE, 0);
    int idx1 = m_group->push_signal("SST::CONFIG_LEVEL:LEVEL", GEOPM_DOMAIN_PACKAGE, 1);
    EXPECT_NE(idx0, idx1);

    uint32_t result = 0;

    //uint64_t mask = 0xFF0000

    // first batch
    {
    EXPECT_CALL(*m_sstio, read_batch());
    m_group->read_batch();
    uint32_t raw0 = 0x1428000;
    uint32_t raw1 = 0x1678000;
    uint32_t expected0 = 0x42;
    uint32_t expected1 = 0x67;
    EXPECT_CALL(*m_sstio, sample(CONFIG_LEVEL_0)).WillOnce(Return(raw0));
    EXPECT_CALL(*m_sstio, sample(CONFIG_LEVEL_1)).WillOnce(Return(raw1));
    result = m_group->sample(idx0);
    EXPECT_EQ(expected0, result);
    result = m_group->sample(idx1);
    EXPECT_EQ(expected1, result);
    }

    // sample again without read should get same value
    {
    uint32_t raw0 = 0x1428000;
    uint32_t raw1 = 0x1678000;
    uint32_t expected0 = 0x42;
    uint32_t expected1 = 0x67;
    EXPECT_CALL(*m_sstio, sample(CONFIG_LEVEL_0)).WillOnce(Return(raw0));
    EXPECT_CALL(*m_sstio, sample(CONFIG_LEVEL_1)).WillOnce(Return(raw1));
    result = m_group->sample(idx0);
    EXPECT_EQ(expected0, result);
    result = m_group->sample(idx1);
    EXPECT_EQ(expected1, result);
    }

    // second batch
    {
    EXPECT_CALL(*m_sstio, read_batch());
    m_group->read_batch();
    uint32_t raw0 = 0x1478000;
    uint32_t raw1 = 0x1638000;
    uint32_t expected0 = 0x47;
    uint32_t expected1 = 0x63;
    EXPECT_CALL(*m_sstio, sample(CONFIG_LEVEL_0)).WillOnce(Return(raw0));
    EXPECT_CALL(*m_sstio, sample(CONFIG_LEVEL_1)).WillOnce(Return(raw1));
    result = m_group->sample(idx0);
    EXPECT_EQ(expected0, result);
    result = m_group->sample(idx1);
    EXPECT_EQ(expected1, result);

    }

}

TEST_F(SSTIOGroupTest, sample_highprio_frequency)
{
    enum sst_idx_e {
        FREQ_000,
        FREQ_100
    };

    //int pkg_0_cpu = 0;
    int pkg_1_cpu = 2;

    // mailbox will only be read once even though it supports multiple signals
    EXPECT_CALL(*m_sstio, add_mbox_read(pkg_1_cpu, 0x7F, 0x11, 0x000, 0x00))
        .WillOnce(Return(FREQ_000));
    EXPECT_CALL(*m_sstio, add_mbox_read(pkg_1_cpu, 0x7F, 0x11, 0x100, 0x00))
        .WillOnce(Return(FREQ_100));

    int idx0 = m_group->push_signal("SST::HIGHPRIORITY_FREQUENCY_SSE:0",
                                    GEOPM_DOMAIN_PACKAGE, 1);
    int idx1 = m_group->push_signal("SST::HIGHPRIORITY_FREQUENCY_SSE:1",
                                    GEOPM_DOMAIN_PACKAGE, 1);
    int idx4 = m_group->push_signal("SST::HIGHPRIORITY_FREQUENCY_SSE:4",
                                    GEOPM_DOMAIN_PACKAGE, 1);
    int idx5 = m_group->push_signal("SST::HIGHPRIORITY_FREQUENCY_SSE:5",
                                    GEOPM_DOMAIN_PACKAGE, 1);
    std::set<int> unique_idx { idx0, idx1, idx4, idx5 };
    EXPECT_EQ(4u, unique_idx.size());

    uint32_t result = 0;

    EXPECT_CALL(*m_sstio, read_batch());
    m_group->read_batch();

    uint32_t raw000 = 0x00012322;
    uint32_t raw100 = 0x00012524;
    double expected0 = 0x22 * 1e8;
    double expected1 = 0x23 * 1e8;
    double expected4 = 0x24 * 1e8;
    double expected5 = 0x25 * 1e8;
    EXPECT_CALL(*m_sstio, sample(FREQ_000)).Times(2)
        .WillRepeatedly(Return(raw000));
    EXPECT_CALL(*m_sstio, sample(FREQ_100)).Times(2)
        .WillRepeatedly(Return(raw100));
    result = m_group->sample(idx0);
    EXPECT_EQ(expected0, result);
    result = m_group->sample(idx1);
    EXPECT_EQ(expected1, result);
    result = m_group->sample(idx4);
    EXPECT_EQ(expected4, result);
    result = m_group->sample(idx5);
    EXPECT_EQ(expected5, result);
}

TEST_F(SSTIOGroupTest, adjust_turbo_enable)
{
    enum sst_idx_e {
        TURBO_ENABLE_0,
        TURBO_ENABLE_1
    };

    int pkg_0_cpu = 0;
    int pkg_1_cpu = 2;

    EXPECT_CALL(*m_sstio, add_mbox_write(pkg_0_cpu, 0x7F, 0x02, 0x00, 0x01, 0x00, 0x10000))
        .WillOnce(Return(TURBO_ENABLE_0));
    EXPECT_CALL(*m_sstio, add_mbox_write(pkg_1_cpu, 0x7F, 0x02, 0x00, 0x01, 0x00, 0x10000))
        .WillOnce(Return(TURBO_ENABLE_1));

    int idx0 = m_group->push_control("SST::TURBO_ENABLE", GEOPM_DOMAIN_PACKAGE, 0);
    int idx1 = m_group->push_control("SST::TURBO_ENABLE", GEOPM_DOMAIN_PACKAGE, 1);
    EXPECT_NE(idx0, idx1);

    int shift = 16;  // bit 16
    EXPECT_CALL(*m_sstio, adjust(idx0, 0x1 << shift, 0x10000));
    EXPECT_CALL(*m_sstio, adjust(idx1, 0x0 << shift, 0x10000));
    m_group->adjust(idx0, 0x1);
    m_group->adjust(idx1, 0x0);
}
