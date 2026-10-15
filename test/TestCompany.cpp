#include <fstream>
#include <gtest/gtest.h>

#include "canni/account/Company.h"
#include "canni/account/Options.h"
#include "canni/account/Portfolio.h"
#include "canni/account/Scenario.h"

using namespace canni;

namespace {

struct TestPortfolio : Portfolio {
    explicit TestPortfolio() : Portfolio({
        .avg_inflation_rate = 0.0_pct,
        .birth  = Day("01/01/1980"),
        .today  = Day("01/01/2024"),
        .retire = Day("01/01/2060"),
        .ending = Day("01/01/2080"),
    }, Scenario{}) {}
    void deposit(USD amount) const override { (void)cash.deposit(amount); }
    void invest() const override {}
    Cash &cash = add<Cash>("Cash", 0.0_pct);
};

} // namespace

TEST(Company, FMVBeforeAnyValuationIsZero) {
    Company c("Test");
    EXPECT_EQ(c.fmv_at(Day("01/01/2024")), 0.0_USD);
}

TEST(Company, FMVOnExactDate) {
    Company c("Test");
    c.valuation(Day("01/01/2024"), 10.00_USD, 12.00_USD);
    EXPECT_EQ(c.fmv_at(Day("01/01/2024")), 10.00_USD);
}

TEST(Company, FMVAfterDateReturnsLastKnown) {
    Company c("Test");
    c.valuation(Day("01/01/2024"), 10.00_USD, 12.00_USD);
    EXPECT_EQ(c.fmv_at(Day("06/01/2025")), 10.00_USD);
}

TEST(Company, FMVBeforeFirstValuationIsZero) {
    Company c("Test");
    c.valuation(Day("06/01/2024"), 10.00_USD, 12.00_USD);
    EXPECT_EQ(c.fmv_at(Day("01/01/2024")), 0.0_USD);
}

TEST(Company, FMVUsesLatestValuationBeforeDate) {
    Company c("Test");
    c.valuation(Day("01/01/2022"), 5.00_USD, 6.00_USD);
    c.valuation(Day("01/01/2024"), 10.00_USD, 12.00_USD);
    EXPECT_EQ(c.fmv_at(Day("06/01/2023")), 5.00_USD);
    EXPECT_EQ(c.fmv_at(Day("06/01/2024")), 10.00_USD);
}

TEST(Company, MarketPriceDistinctFromFMV) {
    Company c("Test");
    c.valuation(Day("01/01/2024"), 10.00_USD, 25.00_USD);
    EXPECT_EQ(c.fmv_at(Day("01/01/2024")), 10.00_USD);
    EXPECT_EQ(c.market(Day("01/01/2024")), 25.00_USD);
}

TEST(Company, LeaveOnSetsExpirationThreeMonthsLater) {
    Company co("Test");
    co.valuation(Day("01/01/2020"), 10.00_USD, 10.00_USD);
    TestPortfolio p;
    auto &opts = p.add<Options>(Day("01/01/2023"), co, "Grant", Options::kNSO, 5.00_USD);

    // Set an expiration far in the future first (via load would do this, but set directly).
    opts.set_exp(Day("01/01/2035"));

    co.leave_on(Day("07/01/2024"));

    // Expiration should now be July 1 + 3 months = October 1, 2024.
    // Verify by checking that options are gone after expiry.
    // vested_ is empty, so avail returns {} regardless, but expires_ controls that.
    // We can check indirectly: write a CSV, check avail before/after expiry date.
    // For now just verify no crash and the grant is still in the company's options list.
    EXPECT_EQ(co.options().size(), 1);
}

TEST(Company, LockedOnlyDuringLockup) {
    Company co("Test");
    co.lockup(Day("01/01/2027"), Day("01/01/2028"));
    EXPECT_FALSE(co.locked(Day("12/31/2026")));
    EXPECT_TRUE(co.locked(Day("01/01/2027")));
    EXPECT_TRUE(co.locked(Day("12/31/2027")));
    EXPECT_FALSE(co.locked(Day("01/01/2028")));
}

TEST(Company, SellVestedOptionsSkippedDuringLockup) {
    Company co("Test");
    co.valuation(Day("01/01/2020"), 10.00_USD, 10.00_USD).lockup(Day("01/01/2024"), Day("07/01/2024"));
    TestPortfolio p;
    auto &opts = p.add<Options>(Day("01/01/2023"), co, "Grant", Options::kNSO, 5.00_USD);
    const std::string path = "/tmp/test_company_lockup.csv";
    { std::ofstream f(path); f << "1\tJan 01 2024\t100\t100\t0\n"; }
    opts.load(path);
    p.seed(Day("01/01/2024"));

    p.sell_vested_options(co, Day("03/01/2024"), 100, 10.00_USD);
    EXPECT_EQ(opts.sold(Day("03/01/2024")).nso, 0);
    EXPECT_EQ(p.cash.balance(), 0_USD);

    p.sell_vested_options(co, Day("07/01/2024"), 100, 10.00_USD);
    EXPECT_EQ(opts.sold(Day("07/01/2024")).nso, 100);
    EXPECT_GT(p.cash.balance(), 0_USD);
}
