#include <fstream>
#include <gtest/gtest.h>

#include "../invest/account/Accounts.h"

namespace {

std::string write_csv(const std::string &content) {
    const std::string path = "/tmp/test_account.csv";
    std::ofstream f(path);
    f << content;
    return path;
}

} // namespace

TEST(Accounts, SeedInitializesDate) {
    Accounts accts;
    Cash &cash = accts.add<Cash>("Test", 0.0_pct);
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&cash).balance = 1000_USD;
    EXPECT_EQ(accts.current_day(), Day("01/01/2024"));
    EXPECT_EQ(cash.balance(), 1000_USD);
    EXPECT_EQ(accts.total(), 1000_USD);
}

TEST(Accounts, ImportCSVSetsBalance) {
    const auto path = write_csv(
        "01/01/2024,1000.00\n"
        "02/01/2024,1100.00\n"
    );
    Accounts accts;
    Cash &cash = accts.add<Cash>("Test", 0.0_pct);
    accts.import_csv(path, {Accounts::Balance(cash)});
    EXPECT_EQ(cash.balance(), 1100.00_USD);
    EXPECT_EQ(accts.current_day(), Day("02/01/2024"));
}

TEST(Accounts, ImportCSVSetsPrincipal) {
    const auto path = write_csv("01/01/2024,1200.00,1000.00\n");
    Accounts accts;
    Cash &cash = accts.add<Cash>("Test", 0.0_pct);
    accts.import_csv(path, {Accounts::Balance(cash), Accounts::Principal(cash)});
    EXPECT_EQ(cash.balance(), 1200.00_USD);
    EXPECT_EQ(accts.current_entry(&cash).principal, 1000.00_USD);
}

TEST(Accounts, ImportCSVSkipsInvalidDates) {
    const auto path = write_csv(
        "not-a-date,999.00\n"
        "01/01/2024,1000.00\n"
    );
    Accounts accts;
    Cash &cash = accts.add<Cash>("Test", 0.0_pct);
    accts.import_csv(path, {Accounts::Balance(cash)});
    EXPECT_EQ(accts.totals().size(), 1);
    EXPECT_EQ(accts.current_day(), Day("01/01/2024"));
}

TEST(Accounts, TotalsReturnsPerDateBreakdown) {
    Accounts accts;
    Cash &cash = accts.add<Cash>("Test", 0.0_pct);
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&cash).balance = 1000_USD;
    accts.project_until(Day("03/01/2024"), 1_months);
    const auto totals = accts.totals();
    ASSERT_EQ(totals.size(), 3);
    EXPECT_EQ(totals[0].day, Day("01/01/2024"));
    EXPECT_EQ(totals[1].day, Day("02/01/2024"));
    EXPECT_EQ(totals[2].day, Day("03/01/2024"));
    EXPECT_EQ(totals[0].total, 1000_USD);
}

TEST(Accounts, ProjectWithZeroInterestPreservesBalance) {
    Accounts accts;
    Cash &cash = accts.add<Cash>("Test", 0.0_pct);
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&cash).balance = 1000_USD;
    accts.project_until(Day("01/01/2025"), 1_months);
    EXPECT_EQ(cash.balance(), 1000_USD);
}

TEST(Accounts, ProjectAppliesInterest) {
    Accounts accts;
    Cash &cash = accts.add<Cash>("Test", Interest::Monthly(0.12));
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&cash).balance = 1000_USD;
    accts.project_until(Day("01/01/2025"), 1_months);
    // 12% APY compounded monthly → ~$1120 after one year
    EXPECT_GT(cash.balance(), 1115.00_USD);
    EXPECT_LT(cash.balance(), 1125.00_USD);
}

TEST(Accounts, DepositIncreasesBalance) {
    Accounts accts;
    Cash &cash = accts.add<Cash>("Test", 0.0_pct);
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&cash).balance = 1000_USD;
    (void)cash.deposit(500_USD);
    EXPECT_EQ(cash.balance(), 1500_USD);
    EXPECT_EQ(accts.total(), 1500_USD);
}

TEST(Accounts, WithdrawReducesBalance) {
    Accounts accts;
    Cash &cash = accts.add<Cash>("Test", 0.0_pct);
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&cash).balance = 1000_USD;
    EXPECT_EQ(cash.withdraw(300_USD), 300_USD);
    EXPECT_EQ(cash.balance(), 700_USD);
}

TEST(Accounts, WithdrawCannotExceedBalance) {
    Accounts accts;
    Cash &cash = accts.add<Cash>("Test", 0.0_pct);
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&cash).balance = 500_USD;
    EXPECT_EQ(cash.withdraw(1000_USD), 500_USD);
    EXPECT_EQ(cash.balance(), 0_USD);
}

TEST(Accounts, MarketReturnsOverridesAPY) {
    Accounts accts;
    Stocks &stocks = accts.add<Stocks>("Test", Interest::Monthly(0.0));
    accts.seed(Day("01/01/2024"));
    accts.current_entry(&stocks).balance = 1000_USD;

    // Fixed 20% return for 2024 and 2025, overriding the 0% APY.
    MarketReturns mr;
    mr.base_year = 2024;
    mr.stocks = {0.20, 0.20};
    accts.set_market_returns(std::move(mr));

    accts.project_until(Day("01/01/2025"), 1_months);
    EXPECT_GT(stocks.balance(), 1150.00_USD);
    EXPECT_LT(stocks.balance(), 1250.00_USD);
}
