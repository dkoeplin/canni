#include <fstream>
#include <gtest/gtest.h>

#include "canni/account/MarketReturns.h"
#include "canni/account/Portfolio.h"
#include "canni/account/Scenario.h"

using namespace canni;

namespace {

Portfolio::Params test_params() {
    return {
        .avg_inflation_rate = 3.0_pct,
        .birth  = Day("01/01/1990"),
        .today  = Day("01/01/2024"),
        .retire = Day("01/01/2050"),
        .ending = Day("01/01/2080"),
    };
}

struct TestPortfolio : Portfolio {
    explicit TestPortfolio(const Portfolio::Params &p = test_params())
        : Portfolio(p, Scenario{}) {}
    void deposit(USD amount) const override { (void)cash.deposit(amount); }
    void invest() const override {}
    bool solvent() const override { return true; }
    Cash &cash = add<Cash>("Test", 0.0_pct);
};

std::string write_csv(const std::string &content) {
    const std::string path = "/tmp/test_account.csv";
    std::ofstream f(path);
    f << content;
    return path;
}

} // namespace

TEST(Accounts, SeedInitializesDate) {
    TestPortfolio accts;
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&accts.cash).balance = 1000_USD;
    EXPECT_EQ(accts.current_day(), Day("01/01/2024"));
    EXPECT_EQ(accts.cash.balance(), 1000_USD);
    EXPECT_EQ(accts.total(), 1000_USD);
}

TEST(Accounts, ImportCSVSetsBalance) {
    const auto path = write_csv(
        "01/01/2024,1000.00\n"
        "02/01/2024,1100.00\n"
    );
    TestPortfolio accts;
    accts.import_csv(path, {Portfolio::Balance(accts.cash)});
    EXPECT_EQ(accts.cash.balance(), 1100.00_USD);
    EXPECT_EQ(accts.current_day(), Day("02/01/2024"));
}

TEST(Accounts, ImportCSVSetsPrincipal) {
    const auto path = write_csv("01/01/2024,1200.00,1000.00\n");
    TestPortfolio accts;
    accts.import_csv(path, {Portfolio::Balance(accts.cash), Portfolio::Principal(accts.cash)});
    EXPECT_EQ(accts.cash.balance(), 1200.00_USD);
    EXPECT_EQ(accts.current_entry(&accts.cash).principal, 1000.00_USD);
}

TEST(Accounts, ImportCSVSkipsInvalidDates) {
    const auto path = write_csv(
        "not-a-date,999.00\n"
        "01/01/2024,1000.00\n"
    );
    TestPortfolio accts;
    accts.import_csv(path, {Portfolio::Balance(accts.cash)});
    EXPECT_EQ(accts.totals("", false).points.size(), 1);
    EXPECT_EQ(accts.current_day(), Day("01/01/2024"));
}

TEST(Accounts, TotalsReturnsPerDateBreakdown) {
    TestPortfolio accts;
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&accts.cash).balance = 1000_USD;
    accts.project_until(Day("03/01/2024"), 1_months);
    const auto totals = accts.totals("", false);
    ASSERT_EQ(totals.points.size(), 3);
    EXPECT_EQ(totals.points[0].first, Day("01/01/2024"));
    EXPECT_EQ(totals.points[1].first, Day("02/01/2024"));
    EXPECT_EQ(totals.points[2].first, Day("03/01/2024"));
    EXPECT_NEAR(totals.points[0].second.f64(), 1000.0, 0.01);
}

TEST(Accounts, ProjectWithZeroInterestPreservesBalance) {
    TestPortfolio accts;
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&accts.cash).balance = 1000_USD;
    accts.project_until(Day("01/01/2025"), 1_months);
    EXPECT_EQ(accts.cash.balance(), 1000_USD);
}

TEST(Accounts, ProjectAppliesInterest) {
    struct InterestPortfolio : Portfolio {
        explicit InterestPortfolio(const Portfolio::Params &p = test_params())
            : Portfolio(p, Scenario{}) {}
        void deposit(USD amount) const override { (void)savings.deposit(amount); }
        void invest() const override {}
        bool solvent() const override { return true; }
        Cash &savings = add<Cash>("Test", Interest::Monthly(0.12));
    };
    InterestPortfolio accts;
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&accts.savings).balance = 1000_USD;
    accts.project_until(Day("01/01/2025"), 1_months);
    // 12% APY compounded monthly → ~$1120 after one year
    EXPECT_GT(accts.savings.balance(), 1115.00_USD);
    EXPECT_LT(accts.savings.balance(), 1125.00_USD);
}

TEST(Accounts, DepositIncreasesBalance) {
    TestPortfolio accts;
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&accts.cash).balance = 1000_USD;
    (void)accts.cash.deposit(500_USD);
    EXPECT_EQ(accts.cash.balance(), 1500_USD);
    EXPECT_EQ(accts.total(), 1500_USD);
}

TEST(Accounts, WithdrawReducesBalance) {
    TestPortfolio accts;
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&accts.cash).balance = 1000_USD;
    EXPECT_EQ(accts.cash.withdraw(300_USD), 300_USD);
    EXPECT_EQ(accts.cash.balance(), 700_USD);
}

TEST(Accounts, WithdrawCannotExceedBalance) {
    TestPortfolio accts;
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&accts.cash).balance = 500_USD;
    EXPECT_EQ(accts.cash.withdraw(1000_USD), 500_USD);
    EXPECT_EQ(accts.cash.balance(), 0_USD);
}

TEST(Accounts, MarketReturnsOverridesAPY) {
    struct StocksPortfolio : Portfolio {
        explicit StocksPortfolio(const Portfolio::Params &p) : Portfolio(p, Scenario{}) {}
        void deposit(USD amount) const override { (void)stocks.deposit(amount); }
        void invest() const override {}
        bool solvent() const override { return true; }
        Stocks &stocks = add<Stocks>("Test", Interest::Monthly(0.0));
    };
    std::mt19937 rng(42);
    auto p = test_params();
    p.returns = MarketReturns(2024, 2, {{Account::kStocks, 0.20, 0.0}}, rng);
    StocksPortfolio accts(p);
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&accts.stocks).balance = 1000_USD;
    accts.project_until(Day("01/01/2025"), 1_months);
    EXPECT_GT(accts.stocks.balance(), 1150.00_USD);
    EXPECT_LT(accts.stocks.balance(), 1250.00_USD);
}
