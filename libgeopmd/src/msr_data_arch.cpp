// THIS IS A GENERATED FILE, DO NOT MODIFY, INSTEAD MODIFY: docs/json_data/msr_data_arch.json
// AND RERUN update_data.sh
/*
 * Copyright (c) 2015 - 2024, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.h"

#include <string>

namespace geopm
{
    const std::string arch_msr_json(void)
    {
        static const std::string result = R"(
{
    "msrs": {
        "TIME_STAMP_COUNTER": {
            "offset": "0x10",
            "domain": "cpu",
            "fields": {
                "TIMESTAMP_COUNT": {
                    "begin_bit": 0,
                    "end_bit":   47,
                    "function":  "overflow",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "monotone",
                    "writeable": false,
                    "description": "An always running, monotonically increasing counter that is incremented at a constant rate.  For use as a wall clock timer.",
                    "aggregation": "select_first"
                }
            }
        },
        "MPERF": {
            "offset": "0xE7",
            "domain": "cpu",
            "fields": {
                "MCNT": {
                    "begin_bit": 0,
                    "end_bit":   47,
                    "function":  "overflow",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "monotone",
                    "writeable": false,
                    "aggregation": "sum",
                    "description": "A counter incrementing at the processor's base, maximum performance frequency. This counter cannot measure processor performance when the CPU is inactive."
                }
            }
        },
        "APERF": {
            "offset": "0xE8",
            "domain": "cpu",
            "fields": {
                "ACNT": {
                    "begin_bit": 0,
                    "end_bit":   47,
                    "function":  "overflow",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "monotone",
                    "writeable": false,
                    "aggregation": "sum",
                    "description": "A counter incrementing at the processor's actual frequency. This counter cannot measure processor performance when the CPU is inactive."
                }
            }
        },
        "THERM_INTERRUPT": {
            "offset": "0x19B",
            "domain": "core",
            "fields": {
                "THRESH_1": {
                    "begin_bit": 8,
                    "end_bit":   14,
                    "function":  "scale",
                    "units":     "celsius",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "The temperature at or above which the MSR::THERM_STATUS:THERMAL_THRESH_1_STATUS indicator is set, in degrees below MSR::TEMPERATURE_TARGET:PROCHOT_MIN.",
                    "aggregation": "expect_same"
                },
                "THRESH_2": {
                    "begin_bit": 16,
                    "end_bit":   22,
                    "function":  "scale",
                    "units":     "celsius",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "The temperature at or above which the MSR::THERM_STATUS:THERMAL_THRESH_2_STATUS indicator is set, in degrees below MSR::TEMPERATURE_TARGET:PROCHOT_MIN.",
                    "aggregation": "expect_same"
                }
            }
        },
        "THERM_STATUS": {
            "offset": "0x19C",
            "domain": "core",
            "fields": {
                "THERMAL_STATUS_FLAG": {
                    "begin_bit": 0,
                    "end_bit":   0,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether the core's on-die sensor reads a high temperature (PROCHOT). When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "THERMAL_STATUS_LOG": {
                    "begin_bit": 1,
                    "end_bit":   1,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether the core's on-die sensor has read a high temperature (PROCHOT) since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "PROCHOT_EVENT": {
                    "begin_bit": 2,
                    "end_bit":   2,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether a high temperature (PROCHOT) or forced power reduction (FORCEPR) is being externally asserted. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "PROCHOT_LOG": {
                    "begin_bit": 3,
                    "end_bit":   3,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether a high temperature (PROCHOT) or forced power reduction (FORCEPR) has been externally asserted since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "CRITICAL_TEMP_STATUS": {
                    "begin_bit": 4,
                    "end_bit":   4,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether the core's on-die sensor reads a critical temperature and the system cannot guarantee reliable operation. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "CRITICAL_TEMP_LOG": {
                    "begin_bit": 5,
                    "end_bit":   5,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether the core's on-die sensor has read a critical temperature since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "THERMAL_THRESH_1_STATUS": {
                    "begin_bit": 6,
                    "end_bit":   6,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether the core's on-die sensor reads equal to or hotter than the threshold in MSR::THERM_INTERRUPT:THRESH_1. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "THERMAL_THRESH_1_LOG": {
                    "begin_bit": 7,
                    "end_bit":   7,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether the core's on-die sensor has read equal to or hotter than the threshold in MSR::THERM_INTERRUPT:THRESH_1 since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "THERMAL_THRESH_2_STATUS": {
                    "begin_bit": 8,
                    "end_bit":   8,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether the core's on-die sensor reads equal to or hotter than the threshold in MSR::THERM_INTERRUPT:THRESH_2. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "THERMAL_THRESH_2_LOG": {
                    "begin_bit": 9,
                    "end_bit":   9,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether the core's on-die sensor has read equal to or hotter than the threshold in MSR::THERM_INTERRUPT:THRESH_2 since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "POWER_LIMIT_STATUS": {
                    "begin_bit": 10,
                    "end_bit":   10,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether requested P-States or requested clock duty cycles are not met. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "POWER_NOTIFICATION_LOG": {
                    "begin_bit": 11,
                    "end_bit":   11,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether requested P-States or requested clock duty cycles were not met at some point since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "DIGITAL_READOUT": {
                    "begin_bit": 16,
                    "end_bit":   22,
                    "function":  "scale",
                    "units":     "celsius",
                    "scalar":    1.0,
                    "writeable": false,
                    "behavior":  "variable",
                    "aggregation": "average",
                    "description": "The temperature reading on this core's on-die sensor, in degrees below MSR::TEMPERATURE_TARGET:PROCHOT_MIN."
                },
                "RESOLUTION": {
                    "begin_bit": 27,
                    "end_bit":   30,
                    "function":  "scale",
                    "units":     "celsius",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "The resolution of the sensor that measures MSR::THERM_STATUS:DIGITAL_READOUT temperature.",
                    "aggregation": "expect_same"
                },
                "READING_VALID": {
                    "begin_bit": 31,
                    "end_bit":   31,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether MSR::THERM_STATUS:DIGITAL_READOUT contains a valid temperature readout. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                }
            }
        },
        "MISC_ENABLE": {
            "offset": "0x1A0",
            "domain": "package",
            "fields": {
                "FAST_STRINGS_ENABLE": {
                    "begin_bit": 0,
                    "end_bit":   0,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Enable software control of the fast string feature for REP MOVS/STORS When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "ENHANCED_SPEEDSTEP_TECH_ENABLE": {
                    "begin_bit": 16,
                    "end_bit":   16,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Enable software control of P-States. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "LIMIT_CPUID_MAXVAL": {
                    "begin_bit": 22,
                    "end_bit":   22,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether the operating system does not support usage of the CPUID instruction with functions that require EAX values great than 2. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "TURBO_MODE_DISABLE": {
                    "begin_bit": 38,
                    "end_bit":   38,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether opportunistic operating frequency above the processor's base frequency is disabled. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                }
            }
        },
        "PACKAGE_THERM_INTERRUPT": {
            "offset": "0x1B2",
            "domain": "package",
            "fields": {
                "THRESH_1": {
                    "begin_bit": 8,
                    "end_bit":   14,
                    "function":  "scale",
                    "units":     "celsius",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "The temperature at or above which the MSR::THERM_STATUS:THERMAL_THRESH_1_STATUS indicator is set, in degrees below MSR::TEMPERATURE_TARGET:PROCHOT_MIN.",
                    "aggregation": "expect_same"
                },
                "THRESH_2": {
                    "begin_bit": 16,
                    "end_bit":   22,
                    "function":  "scale",
                    "units":     "celsius",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "The temperature at or above which the MSR::THERM_STATUS:THERMAL_THRESH_2_STATUS indicator is set, in degrees below MSR::TEMPERATURE_TARGET:PROCHOT_MIN.",
                    "aggregation": "expect_same"
                }
            }
        },
        "PACKAGE_THERM_STATUS": {
            "offset": "0x1B1",
            "domain": "package",
            "fields": {
                "THERMAL_STATUS_FLAG": {
                    "begin_bit": 0,
                    "end_bit":   0,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether the package's on-die sensor reads a high temperature (PROCHOT). When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "THERMAL_STATUS_LOG": {
                    "begin_bit": 1,
                    "end_bit":   1,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether the package's on-die sensor has read a high temperature (PROCHOT) since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "PROCHOT_EVENT": {
                    "begin_bit": 2,
                    "end_bit":   2,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether a package high temperature (PROCHOT) or forced power reduction (FORCEPR) is being externally asserted. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "PROCHOT_LOG": {
                    "begin_bit": 3,
                    "end_bit":   3,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether a package high temperature (PROCHOT) or forced power reduction (FORCEPR) has been externally asserted since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "CRITICAL_TEMP_STATUS": {
                    "begin_bit": 4,
                    "end_bit":   4,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether the package's on-die sensor reads a critical temperature and the system cannot guarantee reliable operation. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "CRITICAL_TEMP_LOG": {
                    "begin_bit": 5,
                    "end_bit":   5,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether the package's on-die sensor has read a critical temperature since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "THERMAL_THRESH_1_STATUS": {
                    "begin_bit": 6,
                    "end_bit":   6,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether the package's on-die sensor reads equal to or hotter than the threshold in MSR::PACKAGE_THERM_INTERRUPT:THRESH_1. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "THERMAL_THRESH_1_LOG": {
                    "begin_bit": 7,
                    "end_bit":   7,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether the package's on-die sensor has read equal to or hotter than the threshold in MSR::PACKAGE_THERM_INTERRUPT:THRESH_1 since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "THERMAL_THRESH_2_STATUS": {
                    "begin_bit": 8,
                    "end_bit":   8,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether the package's on-die sensor reads equal to or hotter than the threshold in MSR::PACKAGE_THERM_INTERRUPT:THRESH_2. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "THERMAL_THRESH_2_LOG": {
                    "begin_bit": 9,
                    "end_bit":   9,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether the package's on-die sensor has read equal to or hotter than the threshold in MSR::PACKAGE_THERM_INTERRUPT:THRESH_2 since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "POWER_LIMIT_STATUS": {
                    "begin_bit": 10,
                    "end_bit":   10,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "description": "Indicates whether requested P-States or requested clock duty cycles are not met due to a package power limit. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "POWER_NOTIFICATION_LOG": {
                    "begin_bit": 11,
                    "end_bit":   11,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": true,
                    "description": "Indicates whether requested P-States or requested clock duty cycles were not met due to a package power limit at some point since the last time a zero was written to this control. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "DIGITAL_READOUT": {
                    "begin_bit": 16,
                    "end_bit":   22,
                    "function":  "scale",
                    "units":     "celsius",
                    "scalar":    1.0,
                    "behavior":  "variable",
                    "writeable": false,
                    "aggregation": "average",
                    "description": "The temperature reading on this package's on-die sensor, in degrees below MSR::TEMPERATURE_TARGET:PROCHOT_MIN."
                }
            }
        },
        "IA32_PERFEVTSEL0": {
            "offset": "0x186",
            "domain": "cpu",
            "fields": {
                "EVENT_SELECT": {
                    "begin_bit": 0,
                    "end_bit":   7,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set an event code to select which event logic unit to monitor. This control combined with MSR::IA32_PERFEVTSEL0:UMASK defines which event to count. See https://download.01.org/perfmon for possible input values. Event counts are accumulated in MSR::IA32_PMC0:PERFCTR.",
                    "aggregation": "expect_same"
                },
                "UMASK": {
                    "begin_bit": 8,
                    "end_bit":   15,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set a unit mask to select which event condition to monitor. This control combined with MSR::IA32_PERFEVTSEL0:EVENT_SELECT defines which event to count. See https://download.01.org/perfmon for possible input values. Event counts are accumulated in MSR::IA32_PMC0:PERFCTR.",
                    "aggregation": "expect_same"
                },
                "USR": {
                    "begin_bit": 16,
                    "end_bit":   16,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count events while in user mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "OS": {
                    "begin_bit": 17,
                    "end_bit":   17,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count events while in kernel mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EDGE": {
                    "begin_bit": 18,
                    "end_bit":   18,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "When set, count rising edges of the event signal instead of counting all instances where the event is observed. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "PC": {
                    "begin_bit": 19,
                    "end_bit":   19,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Only applicable prior to the Sandy Bridge microarchitecture. When set, the processor's PMi pins are toggled (on then off in back-to-back clock cycles) when an event is counted. When cleared, only event counter overflows toggle the PMi pins. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "INT": {
                    "begin_bit": 20,
                    "end_bit":   20,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, generate an interrupt when the counter overflows. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "ANYTHREAD": {
                    "begin_bit": 21,
                    "end_bit":   21,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, increment event counts when the event occurs on any hardware thread from the configured thread's core. Otherwise, only increment event counts when the configured thread triggers the event. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN": {
                    "begin_bit": 22,
                    "end_bit":   22,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable the counters selected in MSR::IA32_PERFEVTSEL0 if both this and MSR::PERF_GLOBAL_CTRL:EN_PMC0 are set. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "INV": {
                    "begin_bit": 23,
                    "end_bit":   23,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Indicates whether non-zero MSR::IA32_PERFEVTSEL0:CMASK events should be inverted. When the CMASK is inverted, increment the event count when the number of occurrences is less than the configured cutoff, instead of the default behavior of counting when the number of occurrences is greater than or equal to the cutoff. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "CMASK": {
                    "begin_bit": 24,
                    "end_bit":   31,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set a mask for instances where multiple events are counted in a single clock cycle. When zero, all events are counted. When non-zero, a single event is counted when the number of event occurrences is greater or equal to the set CMASK value.",
                    "aggregation": "expect_same"
                }
            }
        },
        "IA32_PERFEVTSEL1": {
            "offset": "0x187",
            "domain": "cpu",
            "fields": {
                "EVENT_SELECT": {
                    "begin_bit": 0,
                    "end_bit":   7,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set an event code to select which event logic unit to monitor. This control combined with MSR::IA32_PERFEVTSEL1:UMASK defines which event to count. See https://download.01.org/perfmon for possible input values. Event counts are accumulated in MSR::IA32_PMC1:PERFCTR.",
                    "aggregation": "expect_same"
                },
                "UMASK": {
                    "begin_bit": 8,
                    "end_bit":   15,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set a unit mask to select which event condition to monitor. This control combined with MSR::IA32_PERFEVTSEL1:EVENT_SELECT defines which event to count. See https://download.01.org/perfmon for possible input values. Event counts are accumulated in MSR::IA32_PMC1:PERFCTR.",
                    "aggregation": "expect_same"
                },
                "USR": {
                    "begin_bit": 16,
                    "end_bit":   16,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count events while in user mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "OS": {
                    "begin_bit": 17,
                    "end_bit":   17,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count events while in kernel mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EDGE": {
                    "begin_bit": 18,
                    "end_bit":   18,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "When set, count rising edges of the event signal instead of counting all instances where the event is observed. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "PC": {
                    "begin_bit": 19,
                    "end_bit":   19,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Only applicable prior to the Sandy Bridge microarchitecture. When set, the processor's PMi pins are toggled (on then off in back-to-back clock cycles) when an event is counted. When cleared, only event counter overflows toggle the PMi pins. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "INT": {
                    "begin_bit": 20,
                    "end_bit":   20,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, generate an interrupt when the counter overflows. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "ANYTHREAD": {
                    "begin_bit": 21,
                    "end_bit":   21,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, increment event counts when the event occurs on any hardware thread from the configured thread's core. Otherwise, only increment event counts when the configured thread triggers the event. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN": {
                    "begin_bit": 22,
                    "end_bit":   22,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable the counters selected in MSR::IA32_PERFEVTSEL1 if both this and MSR::PERF_GLOBAL_CTRL:EN_PMC1 are set. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "INV": {
                    "begin_bit": 23,
                    "end_bit":   23,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Indicates whether non-zero MSR::IA32_PERFEVTSEL1:CMASK events should be inverted. When the CMASK is inverted, increment the event count when the number of occurrences is less than the configured cutoff, instead of the default behavior of counting when the number of occurrences is greater than or equal to the cutoff. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "CMASK": {
                    "begin_bit": 24,
                    "end_bit":   31,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set a mask for instances where multiple events are counted in a single clock cycle. When zero, all events are counted. When non-zero, a single event is counted when the number of event occurrences is greater or equal to the set CMASK value.",
                    "aggregation": "expect_same"
                }
            }
        },
        "IA32_PERFEVTSEL2": {
            "offset": "0x188",
            "domain": "cpu",
            "fields": {
                "EVENT_SELECT": {
                    "begin_bit": 0,
                    "end_bit":   7,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set an event code to select which event logic unit to monitor. This control combined with MSR::IA32_PERFEVTSEL2:UMASK defines which event to count. See https://download.01.org/perfmon for possible input values. Event counts are accumulated in MSR::IA32_PMC2:PERFCTR.",
                    "aggregation": "expect_same"
                },
                "UMASK": {
                    "begin_bit": 8,
                    "end_bit":   15,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set a unit mask to select which event condition to monitor. This control combined with MSR::IA32_PERFEVTSEL2:EVENT_SELECT defines which event to count. See https://download.01.org/perfmon for possible input values. Event counts are accumulated in MSR::IA32_PMC2:PERFCTR.",
                    "aggregation": "expect_same"
                },
                "USR": {
                    "begin_bit": 16,
                    "end_bit":   16,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count events while in user mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "OS": {
                    "begin_bit": 17,
                    "end_bit":   17,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count events while in kernel mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EDGE": {
                    "begin_bit": 18,
                    "end_bit":   18,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "When set, count rising edges of the event signal instead of counting all instances where the event is observed. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "PC": {
                    "begin_bit": 19,
                    "end_bit":   19,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Only applicable prior to the Sandy Bridge microarchitecture. When set, the processor's PMi pins are toggled (on then off in back-to-back clock cycles) when an event is counted. When cleared, only event counter overflows toggle the PMi pins. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "INT": {
                    "begin_bit": 20,
                    "end_bit":   20,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, generate an interrupt when the counter overflows. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "ANYTHREAD": {
                    "begin_bit": 21,
                    "end_bit":   21,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, increment event counts when the event occurs on any hardware thread from the configured thread's core. Otherwise, only increment event counts when the configured thread triggers the event. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN": {
                    "begin_bit": 22,
                    "end_bit":   22,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable the counters selected in MSR::IA32_PERFEVTSEL2 if both this and MSR::PERF_GLOBAL_CTRL:EN_PMC2 are set. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "INV": {
                    "begin_bit": 23,
                    "end_bit":   23,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Indicates whether non-zero MSR::IA32_PERFEVTSEL2:CMASK events should be inverted. When the CMASK is inverted, increment the event count when the number of occurrences is less than the configured cutoff, instead of the default behavior of counting when the number of occurrences is greater than or equal to the cutoff. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "CMASK": {
                    "begin_bit": 24,
                    "end_bit":   31,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set a mask for instances where multiple events are counted in a single clock cycle. When zero, all events are counted. When non-zero, a single event is counted when the number of event occurrences is greater or equal to the set CMASK value.",
                    "aggregation": "expect_same"
                }
            }
        },
        "IA32_PERFEVTSEL3": {
            "offset": "0x189",
            "domain": "cpu",
            "fields": {
                "EVENT_SELECT": {
                    "begin_bit": 0,
                    "end_bit":   7,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set an event code to select which event logic unit to monitor. This control combined with MSR::IA32_PERFEVTSEL3:UMASK defines which event to count. See https://download.01.org/perfmon for possible input values. Event counts are accumulated in MSR::IA32_PMC3:PERFCTR.",
                    "aggregation": "expect_same"
                },
                "UMASK": {
                    "begin_bit": 8,
                    "end_bit":   15,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set a unit mask to select which event condition to monitor. This control combined with MSR::IA32_PERFEVTSEL3:EVENT_SELECT defines which event to count. See https://download.01.org/perfmon for possible input values. Event counts are accumulated in MSR::IA32_PMC3:PERFCTR.",
                    "aggregation": "expect_same"
                },
                "USR": {
                    "begin_bit": 16,
                    "end_bit":   16,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count events while in user mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "OS": {
                    "begin_bit": 17,
                    "end_bit":   17,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count events while in kernel mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EDGE": {
                    "begin_bit": 18,
                    "end_bit":   18,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "When set, count rising edges of the event signal instead of counting all instances where the event is observed. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "PC": {
                    "begin_bit": 19,
                    "end_bit":   19,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Only applicable prior to the Sandy Bridge microarchitecture. When set, the processor's PMi pins are toggled (on then off in back-to-back clock cycles) when an event is counted. When cleared, only event counter overflows toggle the PMi pins. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "INT": {
                    "begin_bit": 20,
                    "end_bit":   20,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, generate an interrupt when the counter overflows. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "ANYTHREAD": {
                    "begin_bit": 21,
                    "end_bit":   21,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, increment event counts when the event occurs on any hardware thread from the configured thread's core. Otherwise, only increment event counts when the configured thread triggers the event. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN": {
                    "begin_bit": 22,
                    "end_bit":   22,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable the counters selected in MSR::IA32_PERFEVTSEL3 if both this and MSR::PERF_GLOBAL_CTRL:EN_PMC3 are set. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "INV": {
                    "begin_bit": 23,
                    "end_bit":   23,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Indicates whether non-zero MSR::IA32_PERFEVTSEL3:CMASK events should be inverted. When the CMASK is inverted, increment the event count when the number of occurrences is less than the configured cutoff, instead of the default behavior of counting when the number of occurrences is greater than or equal to the cutoff. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "CMASK": {
                    "begin_bit": 24,
                    "end_bit":   31,
                    "function":  "scale",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Set a mask for instances where multiple events are counted in a single clock cycle. When zero, all events are counted. When non-zero, a single event is counted when the number of event occurrences is greater or equal to the set CMASK value.",
                    "aggregation": "expect_same"
                }
            }
        },
        "IA32_PMC0": {
            "offset": "0xC1",
            "domain": "cpu",
            "fields": {
                "PERFCTR": {
                    "begin_bit": 0,
                    "end_bit":   47,
                    "function":  "overflow",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "monotone",
                    "writeable": false,
                    "aggregation": "sum",
                    "description": "The count of events detected by MSR::IA32_PERFEVTSEL0."
                }
            }
        },
        "IA32_PMC1": {
            "offset": "0xC2",
            "domain": "cpu",
            "fields": {
                "PERFCTR": {
                    "begin_bit": 0,
                    "end_bit":   47,
                    "function":  "overflow",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "monotone",
                    "writeable": false,
                    "aggregation": "sum",
                    "description": "The count of events detected by MSR::IA32_PERFEVTSEL1."
                }
            }
        },
        "IA32_PMC2": {
            "offset": "0xC3",
            "domain": "cpu",
            "fields": {
                "PERFCTR": {
                    "begin_bit": 0,
                    "end_bit":   47,
                    "function":  "overflow",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "monotone",
                    "writeable": false,
                    "aggregation": "sum",
                    "description": "The count of events detected by MSR::IA32_PERFEVTSEL2."
                }
            }
        },
        "IA32_PMC3": {
            "offset": "0xC4",
            "domain": "cpu",
            "fields": {
                "PERFCTR": {
                    "begin_bit": 0,
                    "end_bit":   47,
                    "function":  "overflow",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "monotone",
                    "writeable": false,
                    "aggregation": "sum",
                    "description": "The count of events detected by MSR::IA32_PERFEVTSEL3."
                }
            }
        },
        "FIXED_CTR0": {
            "offset": "0x309",
            "domain": "cpu",
            "fields": {
                "INST_RETIRED_ANY": {
                    "begin_bit": 0,
                    "end_bit":   39,
                    "function":  "overflow",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "monotone",
                    "writeable": false,
                    "aggregation": "sum",
                    "description": "The count of the number of instructions executed."
                }
            }
        },
        "FIXED_CTR1": {
            "offset": "0x30A",
            "domain": "cpu",
            "fields": {
                "CPU_CLK_UNHALTED_THREAD": {
                    "begin_bit": 0,
                    "end_bit":   39,
                    "function":  "overflow",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "monotone",
                    "writeable": false,
                    "aggregation": "sum",
                    "description": "The count of the number of cycles while the logical processor is not in a halt state.  The count rate may change based on core frequency."
                }
            }
        },
        "FIXED_CTR2": {
            "offset": "0x30B",
            "domain": "cpu",
            "fields": {
                "CPU_CLK_UNHALTED_REF_TSC": {
                    "begin_bit": 0,
                    "end_bit":   39,
                    "function":  "overflow",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "monotone",
                    "writeable": false,
                    "aggregation": "sum",
                    "description": "The count of the number of cycles while the logical processor is not in a halt state and not in a stop-clock state.  The count rate is fixed at the TIMESTAMP_COUNT rate."
                }
            }
        },
        "FIXED_CTR_CTRL": {
            "offset": "0x38D",
            "domain": "cpu",
            "fields": {
                "EN0_OS": {
                    "begin_bit": 0,
                    "end_bit":   0,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count MSR::FIXED_CTR0:INST_RETIRED_ANY events while in kernel mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN0_USR": {
                    "begin_bit": 1,
                    "end_bit":   1,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count MSR::FIXED_CTR0:INST_RETIRED_ANY events while in user mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN0_PMI": {
                    "begin_bit": 3,
                    "end_bit":   3,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, generate an interrupt when the MSR::FIXED_CTR0:INST_RETIRED_ANY counter overflows. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN1_OS": {
                    "begin_bit": 4,
                    "end_bit":   4,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count MSR::FIXED_CTR1:CPU_CLK_UNHALTED_THREAD events while in kernel mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN1_USR": {
                    "begin_bit": 5,
                    "end_bit":   5,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count MSR::FIXED_CTR1:CPU_CLK_UNHALTED_THREAD events while in user mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN1_PMI": {
                    "begin_bit": 7,
                    "end_bit":   7,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, generate an interrupt when the MSR::FIXED_CTR1:CPU_CLK_UNHALTED_THREAD counter overflows. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN2_OS": {
                    "begin_bit": 8,
                    "end_bit":   8,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count MSR::FIXED_CTR2:CPU_CLK_UNHALTED_REF_TSC events while in kernel mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN2_USR": {
                    "begin_bit": 9,
                    "end_bit":   9,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Count MSR::FIXED_CTR2:CPU_CLK_UNHALTED_REF_TSC events while in user mode. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN2_PMI": {
                    "begin_bit": 11,
                    "end_bit":   11,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "If set, generate an interrupt when the MSR::FIXED_CTR2:CPU_CLK_UNHALTED_REF_TSC counter overflows. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                }
            }
        },
        "PERF_GLOBAL_CTRL": {
            "offset": "0x38F",
            "domain": "cpu",
            "fields": {
                "EN_PMC0": {
                    "begin_bit": 0,
                    "end_bit":   0,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable programmable counter 0 if both this and MSR::IA32_PERFEVTSEL0:EN are set. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN_PMC1": {
                    "begin_bit": 1,
                    "end_bit":   1,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable programmable counter 1 if both this and MSR::IA32_PERFEVTSEL1:EN are set. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN_PMC2": {
                    "begin_bit": 2,
                    "end_bit":   2,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable programmable counter 2 if both this and MSR::IA32_PERFEVTSEL2:EN are set. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN_PMC3": {
                    "begin_bit": 3,
                    "end_bit":   3,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable programmable counter 3 if both this and MSR::IA32_PERFEVTSEL3:EN are set. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN_FIXED_CTR0": {
                    "begin_bit": 32,
                    "end_bit":   32,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable the MSR::FIXED_CTR0:INST_RETIRED_ANY counter. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN_FIXED_CTR1": {
                    "begin_bit": 33,
                    "end_bit":   33,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable the MSR::FIXED_CTR1:CPU_CLK_UNHALTED_THREAD counter. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                },
                "EN_FIXED_CTR2": {
                    "begin_bit": 34,
                    "end_bit":   34,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Enable the MSR::FIXED_CTR2:CPU_CLK_UNHALTED_REF_TSC counter. When reading at a higher level domain than its native domain, it aggregates as the count of all such bits that have been set.",
                    "aggregation": "sum"
                }
            }
        },
        "PERF_GLOBAL_OVF_CTRL": {
            "offset": "0x390",
            "domain": "cpu",
            "fields": {
                "CLEAR_OVF_PMC0": {
                    "begin_bit": 0,
                    "end_bit":   0,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Write 1 to clear the global status bit for PMC0 overflow.",
                    "aggregation": "expect_same"
                },
                "CLEAR_OVF_PMC1": {
                    "begin_bit": 1,
                    "end_bit":   1,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Write 1 to clear the global status bit for PMC1 overflow.",
                    "aggregation": "expect_same"
                },
                "CLEAR_OVF_PMC2": {
                    "begin_bit": 1,
                    "end_bit":   1,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Write 1 to clear the global status bit for PMC2 overflow.",
                    "aggregation": "expect_same"
                },
                "CLEAR_OVF_PMC3": {
                    "begin_bit": 1,
                    "end_bit":   1,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Write 1 to clear the global status bit for PMC3 overflow.",
                    "aggregation": "expect_same"
                },
                "CLEAR_OVF_FIXED_CTR0": {
                    "begin_bit": 32,
                    "end_bit":   32,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Write 1 to clear the global status bit for MSR::FIXED_CTR0:INST_RETIRED_ANY overflow.",
                    "aggregation": "expect_same"
                },
                "CLEAR_OVF_FIXED_CTR1": {
                    "begin_bit": 33,
                    "end_bit":   33,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Write 1 to clear the global status bit for MSR::FIXED_CTR1:CPU_CLK_UNHALTED_THREAD overflow.",
                    "aggregation": "expect_same"
                },
                "CLEAR_OVF_FIXED_CTR2": {
                    "begin_bit": 34,
                    "end_bit":   34,
                    "function":  "logic",
                    "units":     "none",
                    "scalar":    1.0,
                    "behavior":  "label",
                    "writeable": true,
                    "description": "Write 1 to clear the global status bit for MSR::FIXED_CTR2:CPU_CLK_UNHALTED_REF_TSC overflow.",
                    "aggregation": "expect_same"
                }
            }
        }
    }
}
        )";
        return result;
    }
}
