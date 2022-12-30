/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "config.h"

#include "IOUring.hpp"

#include "IOUringFallback.hpp"
#include "IOUringImp.hpp"

namespace geopm
{
    std::shared_ptr<IOUring> make_io_uring(unsigned entries)
    {
#ifdef GEOPM_HAS_IO_URING
        if (IOUringImp::is_supported()) {
            return make_io_uring_imp(entries);
        }
#endif
        return make_io_uring_fallback(entries);
    }
}
