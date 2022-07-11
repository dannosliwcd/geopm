/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SSTCLOSGOVERNOR_HPP_INCLUDE
#define SSTCLOSGOVERNOR_HPP_INCLUDE

#include <memory>
#include <vector>
#include <string>
#include <map>

namespace geopm
{
    class PlatformIO;
    class PlatformTopo;

    class SSTClosGovernor
    {
        public:
            enum ClosLevel {
                HIGH_PRIORITY = 0,
                MEDIUM_HIGH_PRIORITY = 1,
                MEDIUM_LOW_PRIORITY = 2,
                LOW_PRIORITY = 3,
            };

            SSTClosGovernor() = default;
            virtual ~SSTClosGovernor() = default;
            virtual void init_platform_io(void) = 0;
            virtual int clos_domain_type(void) const = 0;
            virtual void adjust_platform(const std::vector<double> &clos_by_core) = 0;
            virtual bool do_write_batch(void) const = 0;

            // No-op if sst-tf is not supported. Mods both tf and cp
            virtual void enable_sst_turbo_prioritization() = 0;
            virtual void disable_sst_turbo_prioritization() = 0;

            // Indicate whether this platform supports SST-TF
            static bool is_supported(PlatformIO &platform_io);

            static std::unique_ptr<SSTClosGovernor> make_unique(void);
            static std::shared_ptr<SSTClosGovernor> make_shared(void);
    };
}

#endif
