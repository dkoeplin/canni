#include <gtest/gtest.h>

#include "canni/account/MarketReturns.h"

using namespace canni;

namespace {

// sigma=0 makes log-normal deterministic: asset return == mean every year.
MarketReturns make_deterministic(double mean, I64 base_year = 2024, I64 years = 10) {
    std::mt19937 rng(42);
    return MarketReturns(base_year, years,
        {{Account::kStocks, mean, 0.0}},
        rng);
}

} // namespace

TEST(MarketReturns, AtReturnsZeroBeforeBaseYear) {
    const auto mr = make_deterministic(0.07);
    EXPECT_DOUBLE_EQ(mr.at(Account::kStocks, 2023), 0.0);
}

TEST(MarketReturns, AtReturnsZeroAfterEnd) {
    const auto mr = make_deterministic(0.07, 2024, 5); // covers 2024-2028
    EXPECT_DOUBLE_EQ(mr.at(Account::kStocks, 2029), 0.0);
}

TEST(MarketReturns, CumulativeReturnsZeroBeforeBaseYear) {
    const auto mr = make_deterministic(0.07);
    EXPECT_DOUBLE_EQ(mr.cumulative(Account::kStocks, 2023), 0.0);
}

TEST(MarketReturns, CumulativeReturnsZeroAfterEnd) {
    const auto mr = make_deterministic(0.07, 2024, 5);
    EXPECT_DOUBLE_EQ(mr.cumulative(Account::kStocks, 2029), 0.0);
}

TEST(MarketReturns, DeterministicAtMatchesMean) {
    // With sigma=0, every year's return equals the mean.
    const auto mr = make_deterministic(0.10);
    EXPECT_NEAR(mr.at(Account::kStocks, 2024), 0.10, 1e-9);
    EXPECT_NEAR(mr.at(Account::kStocks, 2028), 0.10, 1e-9);
}

TEST(MarketReturns, DeterministicCumulativeGrowsCorrectly) {
    // Cumulative multiplier at year N = (1 + mean)^(N - base_year).
    // cumulative[0] == 1.0 (base), cumulative[1] == 1.0 * (1 + 0.10) = 1.10, etc.
    const auto mr = make_deterministic(0.10);
    EXPECT_NEAR(mr.cumulative(Account::kStocks, 2024), 1.0,  1e-9);
    EXPECT_NEAR(mr.cumulative(Account::kStocks, 2025), 1.10, 1e-9);
    EXPECT_NEAR(mr.cumulative(Account::kStocks, 2026), 1.21, 1e-9);
}

TEST(MarketReturns, UnregisteredAssetClassReturnsZero) {
    // Only kStocks registered; kBonds should return 0.
    const auto mr = make_deterministic(0.07);
    EXPECT_DOUBLE_EQ(mr.at(Account::kBonds, 2024), 0.0);
    EXPECT_DOUBLE_EQ(mr.cumulative(Account::kBonds, 2024), 0.0);
}

TEST(MarketReturns, StochasticReturnsMeanConverges) {
    // With enough simulations, the average annual return should be near the mean.
    std::mt19937 rng(0);
    const double mean  = 0.07;
    const double sigma = 0.15;
    const I64 n = 500;
    double sum = 0.0;
    for (I64 i = 0; i < n; ++i) {
        MarketReturns mr(2024, 1, {{Account::kStocks, mean, sigma}}, rng);
        sum += mr.at(Account::kStocks, 2024);
    }
    EXPECT_NEAR(sum / n, mean, 0.02); // within 2% of expected mean
}
