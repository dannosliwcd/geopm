/*
 * Copyright (c) 2015 - 2022, Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <memory>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "FrequencyTimeBalancer.hpp"
#include "FrequencyLimitDetector.hpp"
#include "MockFrequencyLimitDetector.hpp"
#include "geopm/Helper.hpp"

using geopm::FrequencyTimeBalancer;
using ::testing::DoubleEq;
using ::testing::Pointwise;
using ::testing::Return;
using ::testing::_;
using ::testing::Ge;
using ::testing::ElementsAre;

// This CPU core filter always returns false, indicating that no cores should
// be ignored by the balancer.
static bool ignore_no_cores(int)
{
    return false;
}

// This frequency limit detector assumes that all observed core frequencies
// represent the max achievable core frequency on each core.
class PassThroughFrequencyLimitDetector : public geopm::FrequencyLimitDetector
{
    public:
        PassThroughFrequencyLimitDetector(double lp_frequency)
            : m_lp_frequency(lp_frequency)
            , m_observed_core_frequencies()
        {}

        ~PassThroughFrequencyLimitDetector() = default;

        void update_max_frequency_estimates(
                const std::vector<double> &observed_core_frequencies) override
        {
            m_observed_core_frequencies = observed_core_frequencies;
        }

        std::vector<std::pair<unsigned int, double> > get_core_frequency_limits(
                unsigned int core_idx) const override
        {
            return { { m_observed_core_frequencies.size(),
                       m_observed_core_frequencies.at(core_idx) } };
        }

        double get_core_low_priority_frequency(unsigned int) const override
        {
            return m_lp_frequency;
        }

    private:
        double m_lp_frequency;
        std::vector<double> m_observed_core_frequencies;
};

static const double UNCERTAINTY_WINDOW = 0.000;
static const int PACKAGE_COUNT = 1;

class FrequencyTimeBalancerTest : public ::testing::Test
{
};

TEST_F(FrequencyTimeBalancerTest, balance_when_current_frequencies_are_all_unlimited)
{
    auto frequency_limit_detector = geopm::make_unique<PassThroughFrequencyLimitDetector>(1e9);
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, PACKAGE_COUNT, ignore_no_cores, 1e9, 3e9,
        std::move(frequency_limit_detector));
    const std::vector<double> previous_control_frequencies = { 3e9, 3e9, 3e9, 3e9 };
    // For all subtests, the outputs must contain at least one value at the max
    // frequency.

    // Single long-running core
    EXPECT_THAT(balancer->balance_frequencies_by_time({ 1.0, 1.0, 2.0, 1.0 },
                                                      previous_control_frequencies,
                                                      previous_control_frequencies,
                                                      previous_control_frequencies),
                Pointwise(DoubleEq(), { 1.5e9, 1.5e9, 3e9, 1.5e9 }));

    // Stratified run times
    EXPECT_THAT(balancer->balance_frequencies_by_time({ 1.0, 2.0, 3.0, 4.0 },
                                                      previous_control_frequencies,
                                                      previous_control_frequencies,
                                                      previous_control_frequencies),
                Pointwise(DoubleEq(), { 1e9 /* 0.75e9, clamped to freq min */, 1.5e9, 2.25e9, 3e9 }));
}

TEST_F(FrequencyTimeBalancerTest, balance_when_all_frequencies_should_go_unlimited)
{
    auto frequency_limit_detector = geopm::make_unique<PassThroughFrequencyLimitDetector>(1e9);
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, PACKAGE_COUNT, ignore_no_cores, 1e9, 3e9,
        std::move(frequency_limit_detector));
    const std::vector<double> desired_frequencies = { 3e9, 3e9, 3e9, 3e9 };
    EXPECT_THAT(balancer->balance_frequencies_by_time({ 1.33,    1.6, 2.0, 4.0 },
                                                      { 3.0e9, 2.5e9, 2e9, 1e9 },
                                                      { 3.0e9, 2.5e9, 2e9, 1e9 },
                                                      { 3.0e9, 2.5e9, 2e9, 1e9 }),
                Pointwise(DoubleEq(), desired_frequencies));
}

TEST_F(FrequencyTimeBalancerTest, does_not_change_when_already_balanced)
{
    auto frequency_limit_detector = geopm::make_unique<PassThroughFrequencyLimitDetector>(1e9);
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, PACKAGE_COUNT, ignore_no_cores, 1e9, 3e9,
        std::move(frequency_limit_detector));

    const std::vector<double> balanced_frequencies = { 1e9, 3e9, 2e9, 1e9 };
    EXPECT_THAT(balancer->balance_frequencies_by_time({ 2.0, 2.0, 2.0, 2.0 },
                                                      balanced_frequencies,
                                                      balanced_frequencies,
                                                      balanced_frequencies),
                Pointwise(DoubleEq(), balanced_frequencies));
}

TEST_F(FrequencyTimeBalancerTest, independently_balances_subdomains)
{
    // E.g., Separately balancing 2 packages across all cores
    const int subgroup_count = 2;
    auto frequency_limit_detector = geopm::make_unique<PassThroughFrequencyLimitDetector>(1e9);
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, subgroup_count, ignore_no_cores, 1e9, 3e9,
        std::move(frequency_limit_detector));

    {
        const double low_time = 0.1;
        const double high_time = 2 * low_time;
        const double time_ratio = low_time / high_time;
        EXPECT_THAT(balancer->balance_frequencies_by_time({ high_time, low_time, // Group 1
                                                            low_time, high_time }, // Group 2
                                                          { 3e9, 3e9, 3e9, 3e9 },
                                                          { 3e9, 3e9, 3e9, 3e9 },
                                                          { 3e9, 3e9, 3e9, 3e9 }),
                    Pointwise(DoubleEq(), { 3e9,              3e9 * time_ratio,
                                            3e9 * time_ratio, 3e9               }))
            << "Multiple subgroups should be balanced separately";
    }
}

TEST_F(FrequencyTimeBalancerTest, can_ignore_control_domain_indices)
{
    // E.g., may use to ignore a core that should not be balanced (such as a
    // a core in a non-application region)
    auto frequency_limit_detector = geopm::make_unique<PassThroughFrequencyLimitDetector>(1e9);
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, 1, [](int control_index) { return control_index == 0; }, 1e9, 3e9,
        std::move(frequency_limit_detector));

    // Ignore the lagger core
    EXPECT_THAT(balancer->balance_frequencies_by_time({ 4.0, 3.0, 2.0, 1.0 },
                                                      { 2e9 /* ignored */, 3e9, 3e9, 3e9 },
                                                      { 2e9 /* ignored */, 3e9, 3e9, 3e9 },
                                                      { 2e9 /* ignored */, 3e9, 3e9, 3e9 }),
                Pointwise(DoubleEq(), { 2e9, 3e9, 3e9 * 2 / 3, 3e9 * 1 / 3 }))
        << "Expected to ignore index 0 (lagger core)";

    // Ignore the leader core core
    EXPECT_THAT(balancer->balance_frequencies_by_time({ 1.0, 2.0, 3.0, 4.0 },
                                                      { 2e9 /* ignored */, 2e9, 3e9, 3e9 },
                                                      { 2e9 /* ignored */, 2e9, 3e9, 3e9 },
                                                      { 2e9 /* ignored */, 2e9, 3e9, 3e9 }),
                Pointwise(DoubleEq(), { 2e9, 1e9 /* half */, 3e9 * 3 / 4, 3e9 }))
        << "Expected to ignore index 0 (leader core)";
}

TEST_F(FrequencyTimeBalancerTest, does_not_use_capped_cores_as_balance_reference)
{
    // E.g., What if the previous control decision was a misjudgement?
    // If the lagger core is also a freqeuncy-limited core, we shouldn't use it
    // as a balancing reference. Otherwise, we could get stuck in a descent.
    auto frequency_limit_detector = geopm::make_unique<PassThroughFrequencyLimitDetector>(1e9);
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, PACKAGE_COUNT, ignore_no_cores, 1e9, 3e9,
        std::move(frequency_limit_detector));

    // Just one core had a bad setting:
    EXPECT_THAT(balancer->balance_frequencies_by_time({ 4.0, 3.0, 2.0, 1.0 },
                                                      { 1e9, 3e9, 3e9, 3e9 },
                                                      { 1e9, 3e9, 3e9, 3e9 },
                                                      { 1e9, 3e9, 3e9, 3e9 }),
                // Lagger is at 4 seconds, but is also frequency-capped. So
                // treat the next-lagging uncapped time (3.0) as our reference.
                Pointwise(DoubleEq(), { 1e9 * 4/3, 3e9, 3e9 * 2/3, 3e9 / 3}))
        << "Expected to balance against the most-lagging frequency-unlimited core "
           "(time=3.0) and aggressively unlimit other laggers";

    // More than one core had a bad setting:
    EXPECT_THAT(balancer->balance_frequencies_by_time({ 4.0, 3.0, 2.0, 1.0 },
                                                      { 1e9, 1e9, 3e9, 3e9 },
                                                      { 1e9, 1e9, 3e9, 3e9 },
                                                      { 1e9, 1e9, 3e9, 3e9 }),
                // Top 2 laggers are frequency-capped. So treat the
                // next-lagging uncapped time (2.0) as our reference.
                Pointwise(DoubleEq(), { 1e9 * 4/2, 1e9 * 3/2, 3e9, 3e9 * 1/2}))
        << "Expected to balance against the most-lagging frequency-unlimited core "
           "(time=2.0) and aggressively unlimit other laggers";
}

TEST_F(FrequencyTimeBalancerTest, resets_to_baseline_if_invariants_are_violated)
{
    // This helper always chooses at least one unlimited frequency, and its
    // performance objective assumes that there is at least always one
    // unlimited frequency. If for any reason that is not true, the helper
    // should fix that issue in the current decision.
    auto frequency_limit_detector = geopm::make_unique<PassThroughFrequencyLimitDetector>(1e9);
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, PACKAGE_COUNT, ignore_no_cores, 1e9, 5e9,
        std::move(frequency_limit_detector));

    // No cores currently have a max frequency control (5e9) setting
    EXPECT_THAT(balancer->balance_frequencies_by_time({ 4.0, 3.0, 2.0, 1.0 },
                                                      { 4e9, 3e9, 2e9, 1e9 },
                                                      { 4e9, 3e9, 2e9, 1e9 },
                                                      { 4e9, 3e9, 2e9, 1e9 }),
                Pointwise(DoubleEq(), { 5e9, 5e9, 5e9, 5e9 }))
        << "Expected to reset all cores to unlimited frequency when the previous "
           "state had no frequency-unlimited cores.";
}

TEST_F(FrequencyTimeBalancerTest, no_time_spent_in_balancing_regions)
{
    auto frequency_limit_detector = geopm::make_unique<PassThroughFrequencyLimitDetector>(1e9);
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, PACKAGE_COUNT, ignore_no_cores, 0.9e9, 3e9,
        std::move(frequency_limit_detector));

    EXPECT_THAT(balancer->balance_frequencies_by_time({ 0, 0, 0, 0 },
                                                      { 3e9, 2e9, 2e9, 2e9 },
                                                      { 3e9, 2e9, 2e9, 2e9 },
                                                      { 3e9, 2e9, 2e9, 2e9 }),
                Pointwise(DoubleEq(), { 3e9, 3e9, 3e9, 3e9 }))
        << "Expected maximum frequencies when no cores have time data.";

    EXPECT_THAT(balancer->balance_frequencies_by_time({ 3, 2, 1, 0 },
                                                      { 3e9, 2e9, 3e9, 2e9 },
                                                      { 3e9, 2e9, 3e9, 2e9 },
                                                      { 3e9, 2e9, 3e9, 2e9 }),
                Pointwise(DoubleEq(), { 3e9, 2e9 * 2/3, 3e9 / 3, 0.9e9 }))
        << "Expected a core with no time data to have the low priority frequency.";
}

// Negative time? This can occur when deriving a time signal by subtracting one
// noisy time signal from another noisy time signal. Make sure we can handle it
// since it seems likely our callers may encounter this.
TEST_F(FrequencyTimeBalancerTest, negative_time_spent_in_balancing_regions)
{
    auto frequency_limit_detector = geopm::make_unique<PassThroughFrequencyLimitDetector>(1e9);
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, PACKAGE_COUNT, ignore_no_cores, 0.9e9, 4e9,
        std::move(frequency_limit_detector));

    // All negative times
    EXPECT_THAT(balancer->balance_frequencies_by_time({ -1, -2, -3, -4 },
                                                      { 4e9, 1e9, 1e9, 1e9 },
                                                      { 4e9, 1e9, 1e9, 1e9 },
                                                      { 4e9, 1e9, 1e9, 1e9 }),
                Pointwise(DoubleEq(), { 4e9, 1e9 * 2/1, 1e9 * 3/1, 1e9 * 4/1 }))
        << "Expected to balance against the greatest time when all times are negative.";

    // Some negative times
    EXPECT_THAT(balancer->balance_frequencies_by_time({ 3, 2, 1, -1 },
                                                      { 4e9, 2e9, 3e9, 2e9 },
                                                      { 4e9, 2e9, 3e9, 2e9 },
                                                      { 4e9, 2e9, 3e9, 2e9 }),
                Pointwise(DoubleEq(), { 4e9, 2e9 * 2/3, 3e9 / 3, 0.9e9 }))
        << "Expected a core with negative time data to have the low priority frequency.";
}

// Tests the case where the shortest critical path lands on a high-priority core
//
//       |
//       |------.  _______ (critical path) Freq increased by going HP
// core  |       \/
// time  |       /\_______ Freq dropped by going LP
//       |------'
//       |________________
//        before   after
//       control   control
TEST_F(FrequencyTimeBalancerTest, selects_high_priority_critical_path)
{
    auto mock_frequency_limit_detector = geopm::make_unique<MockFrequencyLimitDetector>();
    auto frequency_limit_detector = mock_frequency_limit_detector.get();
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, PACKAGE_COUNT, ignore_no_cores, 0.5e9, 5e9,
        std::move(mock_frequency_limit_detector));

    std::vector<double> control_frequencies { 5e9, 5e9, 5e9, 5e9 };
    std::vector<double> observed_frequencies { 2e9, 2e9, 2e9, 2e9 };
    EXPECT_CALL(*frequency_limit_detector, update_max_frequency_estimates(
                Pointwise(DoubleEq(), observed_frequencies))).Times(1);

    std::vector<std::pair<unsigned int, double> > hp_frequency_tradeoffs = {
        {1, 5e9}, {2, 4e9}, {3, 3e9}, {4, 2e9}
    };
    EXPECT_CALL(*frequency_limit_detector, get_core_frequency_limits(_))
        .WillRepeatedly(Return(hp_frequency_tradeoffs));
    EXPECT_CALL(*frequency_limit_detector, get_core_low_priority_frequency(_))
        .WillRepeatedly(Return(1e9));

    std::vector<double> initial_times { 4 /* initial crit path */, 0.5, 0.5, 0.5 };
    EXPECT_THAT(balancer->balance_frequencies_by_time(initial_times,
                                                      control_frequencies,
                                                      observed_frequencies,
                                                      observed_frequencies),
                Pointwise(DoubleEq(), { 5e9, 2e9*0.5/1.6, 2e9*0.5/1.6, 2e9*0.5/1.6 }))
        << "Expected 1 core to be recommended at a high priority frequency.";
    EXPECT_THAT(balancer->get_target_times(),
                Pointwise(DoubleEq(), { 1.6 }))
        << "Expected the critical path time to come from core 0 at 5 GHz";
    // Reason we expect 1 high priority core. Consider the following best-perf
    // cases (ignoring that we balance further with additional per-core throttling):
    // If 1:  Expected times are { 2/5*4, 2/1*0.5, 2/1*0.5, 2/1*0.5 }
    //                             `-- Crit path = 1.6
    // If 2:  Expected times are { 2/4*4, 2/4*0.5, 2/1*0.5, 2/1*0.5 }
    //                             |      `- (Improved non-crit-path time)
    //                             `-- Crit path = 2.0
    // If 3:  Expected times are { 2/3*4, 2/3*0.5, 2/3*0.5, 2/1*0.5 }
    //                             `-- Crit path = 2.7
    // If 4:  Expected times are { 2/2*4, 2/2*0.5, 2/2*0.5, 2/2*0.5 }
    //                             `-- Crit path = 4 (where we started)
    // Case 1 has the least crit path time.
}

// Tests the case where the shortest critical path lands on a low-priority core
//
//       |
//       |------.
// core  |       `-------- (critical path) Freq dropped by going LP
// time  |       _________ Freq increased by going HP
//       |------'
//       |________________
//        before   after
//       control   control
TEST_F(FrequencyTimeBalancerTest, selects_low_priority_critical_path)
{
    auto mock_frequency_limit_detector = geopm::make_unique<MockFrequencyLimitDetector>();
    auto frequency_limit_detector = mock_frequency_limit_detector.get();
    auto balancer = FrequencyTimeBalancer::make_unique(
        UNCERTAINTY_WINDOW, PACKAGE_COUNT, ignore_no_cores, 0.5e9, 5e9,
        std::move(mock_frequency_limit_detector));

    std::vector<double> control_frequencies { 5e9, 5e9, 5e9, 5e9 };
    std::vector<double> observed_frequencies { 2e9, 2e9, 2e9, 2e9 };
    EXPECT_CALL(*frequency_limit_detector, update_max_frequency_estimates(
                Pointwise(DoubleEq(), observed_frequencies))).Times(1);

    std::vector<std::pair<unsigned int, double> > hp_frequency_tradeoffs = {
        {1, 5e9}, {2, 4e9}, {3, 3e9}, {4, 2e9}
    };
    EXPECT_CALL(*frequency_limit_detector, get_core_frequency_limits(_))
        .WillRepeatedly(Return(hp_frequency_tradeoffs));
    EXPECT_CALL(*frequency_limit_detector, get_core_low_priority_frequency(_))
        .WillRepeatedly(Return(1e9));

    std::vector<double> initial_times { 3, 1, 3, 3 };
    EXPECT_THAT(balancer->balance_frequencies_by_time(initial_times,
                                                      control_frequencies,
                                                      observed_frequencies,
                                                      observed_frequencies),
                ElementsAre(Ge(3e9), DoubleEq(1e9), Ge(3e9), Ge(3e9)))
        << "Expected 3 cores to be recommended at the high priority frequency.";
    EXPECT_THAT(balancer->get_target_times(),
                ElementsAre(DoubleEq(2.0)))
        << "Expected the critical path time to come from core 1 at 1 GHz";
    // Reason we expect 3 high priority cores. Consider the following best-perf
    // cases (ignoring that we balance further with additional per-core throttling):
    // If 1:  Expected times are { 2/5*3, 2/1*1, 2/1*3, 2/1*3 }
    //                                           `------`----- Crit path = 6.0
    //                           (Crit path could be any 2 of core 0, 2, or 3 in
    //                            low priority -- same result either way)
    // If 2:  Expected times are { 2/4*3, 2/1*1, 2/4*3, 2/1*3 }
    //                                                  `----- Crit path = 6.0
    //                           (Crit path could be any 1 of core 0, 2, or 3 in
    //                            low priority -- same result either way)
    // If 3:  Expected times are { 2/3*3, 2/1*1, 2/3*3, 2/3*3 }
    //                                    `------------------- Crit path = 2.0
    // If 4:  Expected times are { 2/2*3, 2/2*1, 2/2*3, 2/2*3 }
    //                             `-------------`------`----- Crit path = 3.0 (where we started)
    // Case 3 has the least crit path time.
}
