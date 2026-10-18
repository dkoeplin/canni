#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

#include "canni/account/Portfolio.h"
#include "canni/account/Scenario.h"
#include "canni/event/Event.h"
#include "canni/plot/Plotting.h"
#include "canni/tools/Experiment.h"
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

std::string read_file(const std::string &path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
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

TEST(Sweep, ValuesIncludesMax) {
    const auto xs = values(Range<USD>{.min = 0_USD, .max = 100_USD, .step = 25_USD});
    EXPECT_EQ(xs, (std::vector{0_USD, 25_USD, 50_USD, 75_USD, 100_USD}));
}

TEST(Sweep, OneSeriesPerVariant) {
    // The metric just reports the scenario name, so no projection is needed.
    const auto results = sweep(std::vector<I64>{1, 2}, std::vector<Variant<I64>>{
        {"A", [](I64 x) { return Scenario("a" + std::to_string(x), {}); }},
        {"B", [](I64 x) { return Scenario("b" + std::to_string(x), {}); }},
    }, [](const Scenario &s) { return s.name; });
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].name, "A");
    EXPECT_EQ(results[1].name, "B");
    EXPECT_EQ(results[0].points, (std::vector<std::pair<I64, std::string>>{{1, "a1"}, {2, "a2"}}));
    EXPECT_EQ(results[1].points, (std::vector<std::pair<I64, std::string>>{{1, "b1"}, {2, "b2"}}));
}

TEST(Sweep, RetirementPerVariant) {
    const auto results = sweep_retirement<SalaryPortfolio>(salary_params(), std::vector{500_USD, 1000_USD}, {
        {"Spending", [](const USD monthly) { return spend(monthly); }},
    });
    ASSERT_EQ(results.retirement.size(), 1);
    ASSERT_EQ(results.balance.size(), 1);
    EXPECT_EQ(results.retirement[0].name, "Spending");
    EXPECT_EQ(results.balance[0].name, "Spending");
    // Matches min_retirement for each value of the sweep.
    for (U64 i = 0; i < 2; ++i) {
        const auto &[x, day] = results.retirement[0].points[i];
        const auto expected = min_retirement<SalaryPortfolio>(salary_params(), spend(x));
        EXPECT_EQ(day, expected.day);
        EXPECT_EQ(results.balance[0].points[i].second, expected.balance);
    }
    EXPECT_LT(results.retirement[0].points[0].second, results.retirement[0].points[1].second);
}

TEST(Sweep, WriteCSVAlignsSeriesByX) {
    const std::vector<Series<I64, USD>> series = {
        {.name = "First", .points = {{1, 10_USD}, {2, 20_USD}}},
        {.name = "Second, \"quoted\"", .points = {{2, 200_USD}, {3, 300_USD}}},
    };
    write_csv(series, {.title = "TestTools CSV", .x_title = "X"});
    EXPECT_EQ(read_file(output_path("TestTools CSV", "csv")),
              "\"X\",\"First\",\"Second, \"\"quoted\"\"\"\n"
              "1,10.000000,\n"
              "2,20.000000,200.000000\n"
              "3,,300.000000\n");
}
