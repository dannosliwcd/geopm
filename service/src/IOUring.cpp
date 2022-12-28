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
    std::unique_ptr<IOUring> make_io_uring(unsigned entries)
    {
#ifdef GEOPM_HAS_IO_URING
        return make_io_uring_imp(entries);
#else
        return make_io_uring_fallback(entries);
#endif
    }
}
