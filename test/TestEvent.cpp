#include <gtest/gtest.h>

#include "canni/account/Portfolio.h"
#include "canni/account/Scenario.h"
#include "canni/event/Event.h"

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
    bool solvent() const override { return true; }
    Cash &cash = add<Cash>("Cash", 0.0_pct);
};

} // namespace

// ────────────────────────────────────────────────────────────
// Once
// ────────────────────────────────────────────────────────────

TEST(Once, FiresOnExactDate) {
    TestPortfolio p;
    int count = 0;
    p.add<Once>("test", Day("06/01/2024"), [&](Day) { ++count; });
    p.seed(Day("01/01/2024"));
    p.project_until(Day("12/01/2024"), 1_months);
    EXPECT_EQ(count, 1);
}

TEST(Once, DoesNotFireBeforeDate) {
    TestPortfolio p;
    int count = 0;
    p.add<Once>("test", Day("06/01/2025"), [&](Day) { ++count; });
    p.seed(Day("01/01/2024"));
    p.project_until(Day("12/01/2024"), 1_months);
    EXPECT_EQ(count, 0);
}

TEST(Once, FiresExactlyOnce) {
    TestPortfolio p;
    int count = 0;
    p.add<Once>("test", Day("03/01/2024"), [&](Day) { ++count; });
    p.seed(Day("01/01/2024"));
    p.project_until(Day("12/01/2025"), 1_months);
    EXPECT_EQ(count, 1);
}

TEST(Once, FiresOnFirstDateOnOrAfterTarget) {
    // Target is Feb 15; projection steps monthly on the 1st, so first hit is Mar 1.
    TestPortfolio p;
    Day fired_on = Day("01/01/1900");
    p.add<Once>("test", Day("02/15/2024"), [&](Day d) { fired_on = d; });
    p.seed(Day("01/01/2024"));
    p.project_until(Day("06/01/2024"), 1_months);
    EXPECT_EQ(fired_on, Day("03/01/2024"));
}

// ────────────────────────────────────────────────────────────
// Monthly
// ────────────────────────────────────────────────────────────

TEST(Monthly, FiresOncePerMonthlyStep) {
    // Seed Jan 1; first eval Feb 1, last eval Jun 1 = 5 fires.
    TestPortfolio p;
    int count = 0;
    p.add<Monthly>("test", Day("01/01/2024"), [&](Day) { ++count; });
    p.seed(Day("01/01/2024"));
    p.project_until(Day("06/01/2024"), 1_months);
    EXPECT_EQ(count, 5); // Feb, Mar, Apr, May, Jun
}

TEST(Monthly, CatchesUpWhenProjectionSkipsAhead) {
    // Seed Jan 1; 3-month steps land on Apr, Jul, Oct, Jan 2025.
    // Apr: fires once (first eval). Jul: catches up May, Jun, Jul = 3.
    // Oct: Aug, Sep, Oct = 3. Jan 2025: Nov, Dec, Jan = 3. Total = 10.
    TestPortfolio p;
    int count = 0;
    p.add<Monthly>("test", Day("01/01/2024"), [&](Day) { ++count; });
    p.seed(Day("01/01/2024"));
    p.project_until(Day("01/01/2025"), 3_months);
    EXPECT_EQ(count, 10);
}

TEST(Monthly, DoesNotFireBeforeStart) {
    TestPortfolio p;
    int count = 0;
    p.add<Monthly>("test", Day("06/01/2024"), [&](Day) { ++count; });
    p.seed(Day("01/01/2024"));
    p.project_until(Day("04/01/2024"), 1_months);
    EXPECT_EQ(count, 0);
}

TEST(Monthly, RespectsEndingDate) {
    // Fires at Feb, Mar, Apr 2024 (ending Apr 1 is inclusive).
    TestPortfolio p;
    int count = 0;
    auto &ev = p.add<Monthly>("test", Day("01/01/2024"), [&](Day) { ++count; });
    ev.ending(Day("04/01/2024"));
    p.seed(Day("01/01/2024"));
    p.project_until(Day("12/01/2024"), 1_months);
    EXPECT_EQ(count, 3); // Feb, Mar, Apr only
}

// ────────────────────────────────────────────────────────────
// Yearly
// ────────────────────────────────────────────────────────────

TEST(Yearly, FiresOncePerYear) {
    // Seed Jan 1 2024; first evaluate is Feb 1 2024, then Feb 1 2025, 2026, 2027 = 4 fires.
    TestPortfolio p;
    int count = 0;
    p.add<Yearly>("test", Day("01/01/2024"), [&](Day) { ++count; });
    p.seed(Day("01/01/2024"));
    p.project_until(Day("06/01/2027"), 1_months);
    EXPECT_EQ(count, 4);
}

TEST(Yearly, CatchesUpWhenProjectionSkipsAhead) {
    // Seed Jan 2024; 2-year steps land on Jan 2026, 2028, 2030.
    // Jan 2026: fires once (first eval). Jan 2028: catches up Jan 2027 + Jan 2028 = 2.
    // Jan 2030: catches up Jan 2029 + Jan 2030 = 2. Total = 5.
    TestPortfolio p;
    int count = 0;
    p.add<Yearly>("test", Day("01/01/2024"), [&](Day) { ++count; });
    p.seed(Day("01/01/2024"));
    p.project_until(Day("01/01/2030"), 2_years);
    EXPECT_EQ(count, 5);
}

TEST(Yearly, DoesNotFireBeforeStart) {
    TestPortfolio p;
    int count = 0;
    p.add<Yearly>("test", Day("01/01/2026"), [&](Day) { ++count; });
    p.seed(Day("01/01/2024"));
    p.project_until(Day("01/01/2025"), 1_months);
    EXPECT_EQ(count, 0);
}

TEST(Yearly, RespectsEndingDate) {
    // Fires at Feb 2024, Feb 2025. Feb 2026 > ending (Mar 1 2026) is false, so fires. = 3.
    TestPortfolio p;
    int count = 0;
    auto &ev = p.add<Yearly>("test", Day("01/01/2024"), [&](Day) { ++count; });
    ev.ending(Day("03/01/2026"));
    p.seed(Day("01/01/2024"));
    p.project_until(Day("01/01/2030"), 1_months);
    EXPECT_EQ(count, 3); // Feb 2024, Feb 2025, Feb 2026
}

// ────────────────────────────────────────────────────────────
// Deposit/balance interaction via events
// ────────────────────────────────────────────────────────────

TEST(Monthly, AccumulatesBalanceOverTime) {
    // 11 project steps (Feb–Dec), each fires once = $11,000.
    TestPortfolio p;
    p.seed(Day("01/01/2024"));
    p.add<Monthly>("salary", Day("01/01/2024"), [&](Day) { p.deposit(1000_USD); });
    p.project_until(Day("12/01/2024"), 1_months);
    EXPECT_EQ(p.cash.balance(), 11000_USD);
}

TEST(Once, DepositLandsInCorrectMonth) {
    TestPortfolio p;
    p.seed(Day("01/01/2024"));
    p.add<Once>("bonus", Day("06/01/2024"), [&](Day) { p.deposit(5000_USD); });
    p.project_until(Day("05/01/2024"), 1_months);
    EXPECT_EQ(p.cash.balance(), 0_USD); // not fired yet

    p.project_until(Day("06/01/2024"), 1_months);
    EXPECT_EQ(p.cash.balance(), 5000_USD); // fired on June 1
}
