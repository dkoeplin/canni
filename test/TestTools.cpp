#include <gtest/gtest.h>

#include "canni/account/Portfolio.h"
#include "canni/account/Scenario.h"
#include "canni/event/Event.h"
#include "canni/tools/Retirement.h"

using namespace canni;

namespace {

/// Earns $2,000/month in cash until retirement, with no taxes, interest, or inflation.
struct SalaryPortfolio : Portfolio {
    struct Params {
        Portfolio::Params base;
    };
    explicit SalaryPortfolio(const Params &p, const Scenario &scenario = {})
        : Portfolio(p.base, scenario), params_(p) {
        if (!p.base.history) {
            seed(p.base.today);
        }
        add<Monthly>("Salary", p.base.today + 1_months, [this](const Day d) {
            if (d < params_.base.retire) deposit(2000_USD);
        });
    }
    void deposit(const USD amount) const override { (void)cash.deposit(amount); }
    void invest() const override {}

    const Params params_;
    Cash &cash = add<Cash>("Cash", 0.0_pct);
};

SalaryPortfolio::Params salary_params() {
    return {.base = {
        .avg_inflation_rate = 0.0_pct,
        .birth  = Day("01/01/1990"),
        .today  = Day("01/01/2024"),
        .retire = Day("01/01/2034"),
        .ending = Day("01/01/2034"),
    }};
}

/// Spends [monthly] for the whole projection.
Scenario spend(const USD monthly) {
    return TargetedScenario<SalaryPortfolio>("Spend " + monthly.to_string(), [monthly](SalaryPortfolio &p) {
        p.add<Monthly>("Spending", p.params_.base.today + 1_months, [&p, monthly](Day) {
            p.withdraw("Spending", monthly);
        });
    });
}

} // namespace

TEST(Retirement, MinRetirementFindsFirstPassingDay) {
    // 10 years of $1,000/month spending needs 60 paychecks. Paychecks land on the 1st of each month
    // before retirement, so retiring on 02/01/2029 ends at exactly $0, and 02/02/2029 earns one more.
    const auto result = min_retirement<SalaryPortfolio>(salary_params(), spend(1000_USD));
    EXPECT_EQ(result.day, Day("02/02/2029"));
    EXPECT_EQ(result.balance, 2000_USD);
}

TEST(Retirement, MinRetirementWhenAlreadyRetired) {
    // With savings and no spending, retiring today already leaves a positive balance.
    auto params = salary_params();
    const auto result = min_retirement<SalaryPortfolio>(params, Scenario("Windfall", [](Portfolio &p) {
        p.deposit(1000_USD);
    }));
    EXPECT_EQ(result.day, params.base.today);
    EXPECT_EQ(result.balance, 1000_USD);
}

TEST(Retirement, MinRetirementNeverPasses) {
    // Spending exceeds salary, so no retirement date works.
    const auto params = salary_params();
    const auto result = min_retirement<SalaryPortfolio>(params, spend(3000_USD));
    EXPECT_EQ(result.day, params.base.ending);
    EXPECT_EQ(result.balance, 0_USD);
}
