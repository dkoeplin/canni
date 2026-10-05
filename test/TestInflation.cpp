#include <gtest/gtest.h>

#include "canni/account/Portfolio.h"
#include "canni/account/Scenario.h"

using namespace canni;

namespace {

struct TestPortfolio : Portfolio {
    explicit TestPortfolio(const Portfolio::Params &p) : Portfolio(p, Scenario{}) {}
    void deposit(USD amount) const override { (void)cash.deposit(amount); }
    void invest() const override {}
    bool solvent() const override { return true; }
    Cash &cash = add<Cash>("Cash", 0.0_pct);
};

TestPortfolio make_portfolio(Interest inflation_rate) {
    return TestPortfolio({
        .avg_inflation_rate = inflation_rate,
        .birth  = Day("01/01/1980"),
        .today  = Day("01/01/2024"),
        .retire = Day("01/01/2060"),
        .ending = Day("01/01/2080"),
    });
}

} // namespace

// ────────────────────────────────────────────────────────────
// Zero inflation
// ────────────────────────────────────────────────────────────

TEST(Inflation, ZeroRateSameDayNoChange) {
    auto p = make_portfolio(0.0_pct);
    EXPECT_EQ(p.inflation(Day("01/01/2024"), 1000_USD), 1000_USD);
}

TEST(Inflation, ZeroRateFutureNoChange) {
    auto p = make_portfolio(0.0_pct);
    EXPECT_EQ(p.inflation(Day("01/01/2030"), 1000_USD), 1000_USD);
}

TEST(Inflation, ZeroRateInverseNoChange) {
    auto p = make_portfolio(0.0_pct);
    EXPECT_EQ(p.inflation.inverse(Day("01/01/2030"), 1000_USD), 1000_USD);
}

// ────────────────────────────────────────────────────────────
// Non-zero inflation
// ────────────────────────────────────────────────────────────

TEST(Inflation, PositiveRateIncreasesAmount) {
    auto p = make_portfolio(3.0_pct);
    const USD future = p.inflation(Day("01/01/2025"), 1000_USD);
    EXPECT_GT(future, 1000_USD);
    EXPECT_LT(future, 1040_USD); // ~3% growth
}

TEST(Inflation, LongerHorizonMoreInflation) {
    auto p = make_portfolio(3.0_pct);
    const USD one_year  = p.inflation(Day("01/01/2025"), 1000_USD);
    const USD ten_years = p.inflation(Day("01/01/2034"), 1000_USD);
    EXPECT_LT(one_year, ten_years);
}

// ────────────────────────────────────────────────────────────
// Round-trip: inflate then inverse should recover original
// ────────────────────────────────────────────────────────────

TEST(Inflation, RoundTripZeroInflation) {
    auto p = make_portfolio(0.0_pct);
    const Day future = Day("01/01/2030");
    const USD original = 5000_USD;
    const USD inflated = p.inflation(future, original);
    const USD recovered = p.inflation.inverse(future, inflated);
    EXPECT_EQ(recovered, original);
}

TEST(Inflation, RoundTripNonZeroInflation) {
    auto p = make_portfolio(3.0_pct);
    const Day future = Day("01/01/2034");
    const USD original = 5000_USD;
    const USD inflated = p.inflation(future, original);
    const USD recovered = p.inflation.inverse(future, inflated);
    // inverse() floors to cents, so recovered may be one cent below original
    EXPECT_NEAR(recovered.f64(), original.f64(), 0.02);
}

// ────────────────────────────────────────────────────────────
// Two-argument form: start → end
// ────────────────────────────────────────────────────────────

TEST(Inflation, TwoArgSameStartEndNoChange) {
    auto p = make_portfolio(3.0_pct);
    EXPECT_EQ(p.inflation(Day("01/01/2026"), Day("01/01/2026"), 1000_USD), 1000_USD);
}

TEST(Inflation, TwoArgMatchesSingleArgForSameSpan) {
    auto p = make_portfolio(3.0_pct);
    // inflation(today → future) should equal inflation(start → end) when start == today
    const USD via_one_arg = p.inflation(Day("01/01/2027"), 1000_USD);
    const USD via_two_arg = p.inflation(Day("01/01/2024"), Day("01/01/2027"), 1000_USD);
    EXPECT_EQ(via_one_arg, via_two_arg);
}
