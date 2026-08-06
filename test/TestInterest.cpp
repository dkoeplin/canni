#include <gtest/gtest.h>

#include "Interest.h"
#include "Day.h"
#include "USD.h"

namespace {

Day day(const std::string &str) { return *Day::parse(str, "%m/%d/%Y"); }

} // namespace

TEST(Interest, SimpleNoTime) {
    const auto interest = Interest::Simple(0.05);
    const USD result = interest.estimate(day("01/01/2024"), day("01/01/2024"), USD::dollars(1000));
    EXPECT_EQ(result, USD::dollars(1000));
}

TEST(Interest, SimpleOneYear) {
    // Simple interest: balance * (1 + apy * days/365)
    const auto interest = Interest::Simple(0.10);
    const USD result = interest.estimate(day("01/01/2024"), day("12/31/2024"), USD::dollars(1000));
    // 364 days * (0.10/365) * 1000 ~ $99.73, so total ~ $1099.73
    EXPECT_GT(result, USD::dollars(1090));
    EXPECT_LT(result, USD::dollars(1110));
}

TEST(Interest, MonthlyNoCompoundingPeriods) {
    // When times_compounded == 0, pow(1 + rate, 0) == 1, so result == initial.
    const auto interest = Interest::Monthly(0.12);
    const USD result = interest.estimate(day("01/15/2024"), day("01/25/2024"), USD::dollars(1000));
    EXPECT_EQ(result, USD::dollars(1000));
}

TEST(Interest, MonthlyOneMonth) {
    // Jan 1 to Feb 1: one compounding on Feb 1
    const auto interest = Interest::Monthly(0.12);
    const USD result = interest.estimate(day("01/01/2024"), day("02/01/2024"), USD::dollars(1000));
    // rate_per_period = (1.12)^(1/12) - 1 ~ 0.9489%, so ~$1009.49
    EXPECT_GT(result, USD::dollars(1009));
    EXPECT_LT(result, USD::dollars(1010));
}

TEST(Interest, MonthlyTwelveMonths) {
    // Full year of monthly compounding should match APY definition
    const auto interest = Interest::Monthly(0.12);
    const USD result = interest.estimate(day("01/01/2024"), day("01/01/2025"), USD::dollars(1000));
    // 12 compoundings at monthly rate -> should be close to 1000 * 1.12 = $1120
    EXPECT_GT(result, USD::dollars(1115));
    EXPECT_LT(result, USD::dollars(1125));
}

TEST(Interest, MonthlyCompoundingCount) {
    // times_compounded = months_b - months_a (both 0-indexed month indices).
    // Jan(0) -> Feb(1): 1 compounding. Jan 15(month=0) -> Mar 1(month=2): 2 compoundings.
    const auto interest = Interest::Monthly(0.12);
    const USD one  = interest.estimate(day("01/01/2024"), day("02/01/2024"), USD::dollars(1000));
    const USD two  = interest.estimate(day("01/15/2024"), day("03/01/2024"), USD::dollars(1000));
    EXPECT_LT(one, two); // two compoundings yields more than one
    // two compoundings ~ 1000 * (1.12^(2/12)) ~ $1019.07
    EXPECT_GT(two, USD::dollars(1018));
    EXPECT_LT(two, USD::dollars(1021));
}

TEST(Interest, TimesCompoundedSameMonth) {
    // Jan(0) -> Jan(0): months_b - months_a == 0, so returns initial unchanged.
    const auto interest = Interest::Monthly(0.10);
    const USD result = interest.estimate(day("01/05/2024"), day("01/20/2024"), USD::dollars(1000));
    EXPECT_EQ(result, USD::dollars(1000));
}

TEST(Interest, Compounds) {
    EXPECT_FALSE(Interest::Simple(0.05).compounds());
    EXPECT_TRUE(Interest::Monthly(0.05).compounds());
}

TEST(Interest, PctUDL) {
    const auto i = 8.0_pct;
    EXPECT_DOUBLE_EQ(i.apy(), 0.08);
    EXPECT_TRUE(i.compounds());
}
