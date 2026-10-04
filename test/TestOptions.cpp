#include <fstream>
#include <gtest/gtest.h>

#include "canni/account/Company.h"
#include "canni/account/Options.h"
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
    Cash &cash = add<Cash>("Cash", 0.0_pct);
};

std::string write_nso_csv(const std::string &path, const std::string &rows) {
    std::ofstream f(path);
    f << rows;
    return path;
}

const Company kTestCo = Company("TestCo").valuation(Day("01/01/2020"), 10.00_USD, 10.00_USD);

} // namespace

// ────────────────────────────────────────────────────────────
// Options::load
// ────────────────────────────────────────────────────────────

TEST(Options, LoadPopulatesVested) {
    TestPortfolio p;
    auto &opts = p.add<Options>(kTestCo, "Test", Options::kNSO, 5.00_USD);
    write_nso_csv("/tmp/test_opts_load.tsv",
        "1\tJan 01 2024\t100\t100\t0\n"
        "2\tApr 01 2024\t100\t200\t0\n");
    opts.load("/tmp/test_opts_load.tsv");

    EXPECT_EQ(opts.vested(Day("12/31/2023")).nso, 0);
    EXPECT_EQ(opts.vested(Day("01/01/2024")).nso, 100);
    EXPECT_EQ(opts.vested(Day("04/01/2024")).nso, 200);
}

TEST(Options, LoadEmptyGrantHasNoVested) {
    TestPortfolio p;
    auto &opts = p.add<Options>(kTestCo, "Test", Options::kNSO, 5.00_USD);
    EXPECT_EQ(opts.vested(Day("01/01/2025")).nso, 0);
}

// ────────────────────────────────────────────────────────────
// Options::with_sale
// ────────────────────────────────────────────────────────────

TEST(Options, WithSaleUpdatesSold) {
    TestPortfolio p;
    auto &opts = p.add<Options>(kTestCo, "Test", Options::kNSO, 5.00_USD);
    write_nso_csv("/tmp/test_opts_sale.tsv", "1\tJan 01 2024\t100\t100\t0\n");
    opts.load("/tmp/test_opts_sale.tsv");
    p.seed(Day("01/01/2024"));

    opts.with_sale(Day("06/01/2024"), 15.00_USD, {.nso = 40});

    EXPECT_EQ(opts.sold(Day("06/01/2024")).nso, 40);
    EXPECT_EQ(opts.avail(Day("06/01/2024")).nso, 60);
}

TEST(Options, WithSaleClampsToVested) {
    TestPortfolio p;
    auto &opts = p.add<Options>(kTestCo, "Test", Options::kNSO, 5.00_USD);
    write_nso_csv("/tmp/test_opts_clamp.tsv", "1\tJan 01 2024\t50\t50\t0\n");
    opts.load("/tmp/test_opts_clamp.tsv");
    p.seed(Day("01/01/2024"));

    opts.with_sale(Day("06/01/2024"), 15.00_USD, {.nso = 200}); // request exceeds vested
    EXPECT_EQ(opts.sold(Day("06/01/2024")).nso, 50);             // clamped to 50
    EXPECT_EQ(opts.avail(Day("06/01/2024")).nso, 0);
}

// ────────────────────────────────────────────────────────────
// Options snapshot round-trip
// ────────────────────────────────────────────────────────────

TEST(Options, SnapshotRestoresVestedAndSold) {
    TestPortfolio p1;
    auto &opts = p1.add<Options>(kTestCo, "Test", Options::kNSO, 5.00_USD);
    write_nso_csv("/tmp/test_opts_snap.tsv",
        "1\tJan 01 2024\t200\t200\t0\n");
    opts.load("/tmp/test_opts_snap.tsv");
    p1.seed(Day("01/01/2024"));
    opts.with_sale(Day("06/01/2024"), 15.00_USD, {.nso = 75});

    const auto avail_before = opts.avail(Day("06/01/2024"));
    const auto snap = p1.snapshot();

    // Construct a second portfolio using the snapshot — skip load() and with_sale().
    auto p2_params = test_params();
    p2_params.history = snap;
    TestPortfolio p2(p2_params);
    auto &opts2 = p2.add<Options>(kTestCo, "Test", Options::kNSO, 5.00_USD);

    EXPECT_EQ(opts2.avail(Day("06/01/2024")), avail_before);
    EXPECT_EQ(opts2.vested(Day("01/01/2024")).nso, 200);
    EXPECT_EQ(opts2.sold(Day("06/01/2024")).nso, 75);
}

TEST(Options, SnapshotWithoutLoadHasNoVested) {
    TestPortfolio p1;
    p1.add<Options>(kTestCo, "Test", Options::kNSO, 5.00_USD);
    // No load(), no with_sale — snapshot should have empty vested/sold.
    const auto snap = p1.snapshot();

    auto p2_params = test_params();
    p2_params.history = snap;
    TestPortfolio p2(p2_params);
    auto &opts2 = p2.add<Options>(kTestCo, "Test", Options::kNSO, 5.00_USD);

    EXPECT_EQ(opts2.vested(Day("01/01/2025")).nso, 0);
    EXPECT_EQ(opts2.sold(Day("01/01/2025")).nso, 0);
}

// ────────────────────────────────────────────────────────────
// Portfolio taxes snapshot
// ────────────────────────────────────────────────────────────

TEST(Portfolio, SnapshotCapturesTaxes) {
    TestPortfolio p;
    p.seed(Day("01/01/2024"));
    p.taxes.income(Day("01/01/2024"), 50000_USD);
    p.taxes.withholding(Day("01/01/2024"), 10000_USD);

    const auto snap = p.snapshot();

    EXPECT_EQ(snap.taxes.get(Day("01/01/2024")).income.standard, 50000_USD);
    EXPECT_EQ(snap.taxes.get(Day("01/01/2024")).paid.standard, 10000_USD);
}

TEST(Portfolio, LoadHistoryRestoresTaxes) {
    TestPortfolio p1;
    p1.seed(Day("01/01/2024"));
    p1.taxes.income(Day("01/01/2024"), 80000_USD);
    p1.taxes.withholding(Day("01/01/2024"), 15000_USD);
    const auto snap = p1.snapshot();

    auto p2_params = test_params();
    p2_params.history = snap;
    TestPortfolio p2(p2_params);

    EXPECT_EQ(p2.taxes.get(Day("01/01/2024")).income.standard, 80000_USD);
    EXPECT_EQ(p2.taxes.get(Day("01/01/2024")).paid.standard, 15000_USD);
}

TEST(Portfolio, TaxesNotDoubledAfterSnapshotRestore) {
    TestPortfolio p1;
    p1.seed(Day("01/01/2024"));
    p1.taxes.income(Day("01/01/2024"), 60000_USD);
    const auto snap = p1.snapshot();

    // Simulate what would happen if taxes were re-applied on top of a restore:
    // p2 restores the snapshot taxes, then does NOT add more income entries.
    auto p2_params = test_params();
    p2_params.history = snap;
    TestPortfolio p2(p2_params);

    // Should see 60K exactly — not 120K from double-application.
    EXPECT_EQ(p2.taxes.get(Day("01/01/2024")).income.standard, 60000_USD);
}
