/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "config.h"

#include "IOUring.hpp"

#include <iostream>

#include "IOUringFallback.hpp"
#include "IOUringImp.hpp"

namespace geopm
{
#ifdef GEOPM_HAS_IO_URING
    static void emit_missing_support_warning()
    {
#ifdef GEOPM_DEBUG
        static bool do_emit_warning = true;
        if (do_emit_warning) {
            do_emit_warning = false;
            std::cerr << "Warning: <geopm> GEOPM was built with liburing "
                "enabled, but the system does not support all uring operations "
                "needed by GEOPM. Using non-uring IO instead." << std::endl;
        }
#endif
    }
#endif

    std::shared_ptr<IOUring> make_io_uring(unsigned entries)
    {
#ifdef GEOPM_HAS_IO_URING
        if (IOUringImp::is_supported()) {
            return make_io_uring_imp(entries);
        }
        emit_missing_support_warning();
#endif
        return make_io_uring_fallback(entries);
    }
}
