#include <gtest/gtest.h>

#include "canni/account/Portfolio.h"
#include "canni/account/Scenario.h"
#include "canni/account/Taxes.h"

using namespace canni;

namespace {

// A portfolio with 0% inflation so bracket thresholds stay at their nominal 2026 values.
Portfolio::Params zero_inflation_params() {
    return {
        .avg_inflation_rate = 0.0_pct,
        .birth  = Day("01/01/1980"),
        .today  = Day("01/01/2026"),
        .retire = Day("01/01/2060"),
        .ending = Day("01/01/2080"),
    };
}

struct TestPortfolio : Portfolio {
    explicit TestPortfolio(const Portfolio::Params &p = zero_inflation_params())
        : Portfolio(p, Scenario{}) {}
    void deposit(USD amount) const override { (void)cash.deposit(amount); }
    void invest() const override {}
    bool solvent() const override { return true; }
    Cash &cash = add<Cash>("Cash", 0.0_pct);
};

} // namespace

// ────────────────────────────────────────────────────────────
// Taxes entry recording
// ────────────────────────────────────────────────────────────

TEST(Taxes, GetReturnsEmptyForMissingYear) {
    Taxes t;
    const auto &entry = t.get(Day("01/01/2026"));
    EXPECT_EQ(entry.income.standard, 0_USD);
    EXPECT_EQ(entry.paid.standard, 0_USD);
}

TEST(Taxes, IncomeAccumulatesWithinYear) {
    Taxes t;
    t.income(Day("03/01/2026"), 50000_USD);
    t.income(Day("09/01/2026"), 30000_USD);
    EXPECT_EQ(t.get(Day("12/31/2026")).income.standard, 80000_USD);
}

TEST(Taxes, WithholdingAccumulatesWithinYear) {
    Taxes t;
    t.withholding(Day("03/01/2026"), 8000_USD);
    t.withholding(Day("09/01/2026"), 4000_USD);
    EXPECT_EQ(t.get(Day("12/31/2026")).paid.standard, 12000_USD);
}

TEST(Taxes, EntriesAreSeparateByYear) {
    Taxes t;
    t.income(Day("06/01/2026"), 60000_USD);
    t.income(Day("06/01/2027"), 70000_USD);
    EXPECT_EQ(t.get(Day("12/31/2026")).income.standard, 60000_USD);
    EXPECT_EQ(t.get(Day("12/31/2027")).income.standard, 70000_USD);
}

// ────────────────────────────────────────────────────────────
// 401K limit and RMD
// ────────────────────────────────────────────────────────────

TEST(Taxes, Yearly401kLimitBaseYear) {
    EXPECT_EQ(Taxes::yearly_401k_contribution_limit(2026), 24500_USD);
}

TEST(Taxes, Yearly401kLimitIncrements) {
    EXPECT_EQ(Taxes::yearly_401k_contribution_limit(2027), 25000_USD);
    EXPECT_EQ(Taxes::yearly_401k_contribution_limit(2028), 25500_USD);
}

TEST(Taxes, RMDFactorAtAge73) {
    EXPECT_DOUBLE_EQ(Taxes::required_minimum_distribution(73), 26.5);
}

TEST(Taxes, RMDFactorAtAge74) {
    EXPECT_DOUBLE_EQ(Taxes::required_minimum_distribution(74), 25.5);
}

TEST(Taxes, RMDFactorBelowAge73) {
    EXPECT_DOUBLE_EQ(Taxes::required_minimum_distribution(72), 0.0);
}

// ────────────────────────────────────────────────────────────
// calculate_taxes — federal income tax
// ────────────────────────────────────────────────────────────

TEST(Taxes, ZeroIncomeZeroTax) {
    TestPortfolio p;
    Taxes t;
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.federal_income, 0_USD);
    EXPECT_EQ(result.ltcg, 0_USD);
    EXPECT_EQ(result.niit, 0_USD);
    EXPECT_EQ(result.medicare, 0_USD);
    EXPECT_EQ(result.social_security, 0_USD);
}

TEST(Taxes, DeductionCappedAt60PctOfIncome) {
    // Standard deduction = $32,000, but it's capped at 60% of income.
    // $30,000 income: cap = $18,000 → taxable = $12,000 → tax = $1,200.
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 30000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.federal_income, 1200_USD);
}

TEST(Taxes, IncomeInFirstBracket) {
    // $50,000 income: 60% cap = $30,000 < $32,000 standard → deduction = $30,000
    // Taxable = $20,000 @ 10% = $2,000
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 50000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.federal_income, 2000_USD);
}

TEST(Taxes, IncomeSpansTwoBrackets) {
    // Income = $100,000, deduction = $32,000 → taxable = $68,000
    // 10% on first $24,800 = $2,480
    // 12% on next $43,200 ($68,000 - $24,800) = $5,184
    // Total = $7,664
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 100000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.federal_income, 7664_USD);
}

TEST(Taxes, ItemizedDeductionBeatsStandard) {
    // Large donation → itemized deduction > $32,000 standard deduction
    // Income = $200,000, donation = $60,000 (30% of income, under the 60% cap)
    // Taxable = $200,000 - $60,000 = $140,000
    // 10% on $24,800 + 12% on $76,000 + 22% on $39,200 = $2,480 + $9,120 + $8,624 = $20,224
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 200000_USD);
    t.donation(Day("06/01/2026"), 60000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.federal_income, 20224_USD);
}

// ────────────────────────────────────────────────────────────
// calculate_taxes — NIIT
// ────────────────────────────────────────────────────────────

TEST(Taxes, NIITBelowThreshold) {
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 200000_USD);
    t.dividends(Day("06/01/2026"), 40000_USD); // MAGI = $240,000 < $250,000
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.niit, 0_USD);
}

TEST(Taxes, NIITAboveThreshold) {
    // MAGI = $300,000 > $250,000 threshold
    // NII = $100,000 dividends
    // NIIT base = min($100,000, $300,000 - $250,000) = $50,000
    // NIIT = $50,000 * 0.038 = $1,900
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 200000_USD);
    t.dividends(Day("06/01/2026"), 100000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.niit, 1900_USD);
}

// ────────────────────────────────────────────────────────────
// calculate_taxes — Medicare
// ────────────────────────────────────────────────────────────

TEST(Taxes, MedicareBasicRate) {
    // $200,000 * 1.45% = $2,900
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 200000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.medicare, 2900_USD);
}

TEST(Taxes, MedicareAdditionalAboveThreshold) {
    // $300,000: base = $300,000 * 1.45% = $4,350
    //           additional = $50,000 * 0.9% = $450 → total $4,800
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 300000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.medicare, 4800_USD);
}

// ────────────────────────────────────────────────────────────
// calculate_taxes — Social Security
// ────────────────────────────────────────────────────────────

TEST(Taxes, SocialSecurityBelowWageBase) {
    // $100,000 * 6.2% = $6,200
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 100000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.social_security, 6200_USD);
}

TEST(Taxes, SocialSecurityCappedAtWageBase) {
    // $400,000 income: capped at $180,000 wage base → $180,000 * 6.2% = $11,160
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 400000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.social_security, 11160_USD);
}

// ────────────────────────────────────────────────────────────
// calculate_taxes — LTCG
// ────────────────────────────────────────────────────────────

TEST(Taxes, LTCGBelowZeroRateThreshold) {
    // Ordinary taxable $0, LTCG $80,000 — all within $100,800 zero-rate bracket
    TestPortfolio p;
    Taxes t;
    t.long_term(Day("06/01/2026"), 80000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.ltcg, 0_USD);
}

TEST(Taxes, LTCGStacksOnOrdinaryIncome) {
    // Ordinary taxable = $80,000, LTCG = $60,000 → stacked = $140,000
    // $100,800 threshold: room = $100,800 - $80,000 = $20,800 at 0%
    // remaining = $60,000 - $20,800 = $39,200 at 15% = $5,880
    TestPortfolio p;
    Taxes t;
    t.income(Day("06/01/2026"), 112000_USD); // taxable = $112,000 - $32,000 = $80,000
    t.long_term(Day("06/01/2026"), 60000_USD);
    const auto result = calculate_taxes(p.inflation, t, 2026);
    EXPECT_EQ(result.ltcg, 5880_USD);
}
