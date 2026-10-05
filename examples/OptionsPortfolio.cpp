/// OptionsPortfolio.cpp — example showing equity compensation modeling:
/// vesting schedules, historical sales, snapshot-based Monte Carlo, and fire-sale scenarios.
///
/// Replace placeholder grants, strike prices, and vesting CSVs with your own data.

#include "account/Scenario.h"
#include "canni/account/Options.h"
#include "canni/account/Portfolio.h"
#include "canni/account/Taxes.h"
#include "canni/data/Inflation.h"
#include "canni/plot/Plotting.h"
#include "canni/tools/Compare.h"
#include "canni/tools/MonteCarlo.h"
#include "canni/tools/Retirement.h"

namespace {
using namespace canni;

// ────────────────────────────────────────────────────────────
// Portfolio definition
// ────────────────────────────────────────────────────────────

struct MyFinances : Portfolio {
    struct Params {
        Portfolio::Params base;

        Interest avg_stocks_apy  = 7.0_pct;
        Interest avg_401k_apy    = 7.0_pct;
        Interest avg_roth_apy    = 7.0_pct;

        USD salary           = 150000_USD;
        USD living_expenses  = 70000_USD;
        USD health_insurance = 20000_USD;
    };

    explicit MyFinances(const Params &p, const Scenario &scenario = {})
        : Portfolio(p.base, scenario), params_(p) {

        stocks.set_apy(p.avg_stocks_apy);
        f401k.set_apy(p.avg_401k_apy);
        roth.set_apy(p.avg_roth_apy);
        AcmeCorp.leave_on(p.base.retire);

        if (!p.base.history) {
            // Load vesting schedules from Carta-format CSVs.
            // Columns (NSO/ISO): row, date, new_vested, cumulative, exercised
            // Columns (Mix):     row, date, new_vested, iso_vested, nso_vested, cumulative, exercised
            grant_a.load("../data/grant-a.csv"); // NSO grant, earliest
            grant_b.load("../data/grant-b.csv"); // NSO grant
            grant_c.load("../data/grant-c.csv"); // Mix grant

            // Record any historical sales already made (prevents double-counting on restore).
            grant_a
                .with_sale(Day("06/15/2022"), 18.00_USD, {.nso = 10000})
                .with_sale(Day("12/01/2023"), 22.50_USD, {.nso = 5000});

            // Load historical account balances if available.
            // import_csv("../data/tracking.csv", { Balance(checking), ... });
        }

        add<Yearly>("Taxes", Day("04/01/2027"), [&](Day d) {
            const auto summary = calculate_taxes(inflation, taxes, d.year() - 1);
            if (params_.base.verbose)
                std::cout << "[" << d.year() - 1 << "] Owed: " << summary.net_owed() << "\n";
            (void)withdraw("Taxes", summary.net_owed());
        });

        add<Monthly>("Income & Expenses", p.base.today, [&](Day d) {
            if (d < params_.base.retire) {
                const auto contribution = Taxes::yearly_401k_contribution_limit(d.year()) / 12;
                auto income = inflation.yearly(d, params_.salary) / 12 - contribution;
                const auto ss   = income * 0.062;
                const auto mc   = income * 0.0235;
                const auto with = income * 0.22;
                (void)f401k.deposit(contribution);
                taxes.income(d, income);  taxes.social_security(d, ss);
                taxes.withholding(d, with); taxes.medicare(d, mc);
                deposit(income - with - mc - ss);
            }
            (void)withdraw("Living expenses", inflation(d, params_.living_expenses / 12));
            if (d >= params_.base.retire)
                (void)withdraw("Health insurance", inflation(d, params_.health_insurance / 12));
        });
    }

    bool solvent() const override {
        // Options are illiquid until sold — exclude from solvency check.
        return total() - get<Options>().balance() > 0_USD;
    }

    void deposit(const USD amount) const override { (void)checking.deposit(amount); }

    void invest() const override {
        const auto excess = checking.balance() - inflation(current_day(), 30000_USD);
        if (excess > 0_USD)
            (void)stocks.deposit(checking.withdraw(excess));
    }

    // Accounts
    Cash       &checking = add<Cash>("Checking",    0.0_pct);
    Cash       &savings  = add<Cash>("Savings",     4.5_pct);
    Stocks     &stocks   = add<Stocks>("Index Funds", 7.0_pct);
    Retirement &f401k    = add<Retirement>("401(k)", 7.0_pct, Retirement::kPreTax);
    Retirement &roth     = add<Retirement>("Roth IRA", 7.0_pct, Retirement::kPostTax);

    // Company and option grants — replace with your own company name, grant IDs, and strikes.
    const Company AcmeCorp = Company("AcmeCorp")
        .valuation(Day("01/01/2019"), 5.00_USD,  5.00_USD)   // Seed
        .valuation(Day("01/01/2020"), 8.00_USD,  8.00_USD)   // Series A
        .valuation(Day("01/01/2021"), 12.00_USD, 15.00_USD)  // Series B
        .valuation(Day("01/01/2022"), 18.00_USD, 22.00_USD)  // Series C
        .valuation(Day("01/01/2024"), 25.00_USD, 30.00_USD)  // Series D
        .valuation(Day("01/01/2026"), 35.00_USD, 42.00_USD)  // Series E
    ;

    Options &grant_a = add<Options>(AcmeCorp, "Grant-A", Options::kNSO, 5.00_USD);
    Options &grant_b = add<Options>(AcmeCorp, "Grant-B", Options::kNSO, 12.00_USD);
    Options &grant_c = add<Options>(AcmeCorp, "Grant-C", Options::kMix, 18.00_USD);

    const Params params_;
};

// ────────────────────────────────────────────────────────────
// Scenario modifiers
// ────────────────────────────────────────────────────────────

using Mod = TargetedScenario<MyFinances>;

// Sell [n] vested options at [price] on [when].
Mod sell_options(I64 n, USD price, Day when) {
    return {"Sell " + std::to_string(n) + " options @ " + price.to_string(),
        [n, price, when](MyFinances &s) {
            s.add<Once>("Option Sale", when, [&s, n, price](Day d) {
                s.sell_vested_options(s.AcmeCorp, d, n, price);
            });
        }};
}

// Sell all remaining vested options at retirement for [price] (in today's dollars).
Mod fire_sale(USD price) {
    return {"Fire Sale @ " + price.to_string(), [price](MyFinances &s) {
        s.add<Once>("Fire Sale", s.params_.base.retire, [&s, price](Day d) {
            Options::Count total;
            for (const Options *opt : s.AcmeCorp.options())
                total += opt->avail(d);
            s.sell_vested_options(s.AcmeCorp, d, total.iso + total.nso, s.inflation(d, price));
        });
    }};
}

} // namespace

int main() {
    const Day kBirth("03/10/1992");
    const Day kToday("01/01/2026");

    const MyFinances::Params kBase {
        .base = {
            .avg_inflation_rate = 3.0_pct,
            .birth              = kBirth,
            .today              = kToday,
            .retire             = kToday + 8_years,
            .ending             = kBirth + 90_years,
            .verbose            = true,
        },
        .salary          = 150000_USD,
        .living_expenses = 70000_USD,
    };

    // ── Verbose single run ──────────────────────────────────
    MyFinances base(kBase, fire_sale(40_USD));
    base.project();

    // ── Option sale timing comparison ───────────────────────
    compare<MyFinances>(kBase, {
        fire_sale(30_USD),
        fire_sale(40_USD),
        fire_sale(50_USD),
        sell_options(20000, 45_USD, kToday + 1_years) | fire_sale(35_USD),
    }, {
        .title   = "Options Exit Scenario Comparison",
        .y_title = "Projected Balance (Today's Dollars)"
    });

    // ── Monte Carlo across exit prices ─────────────────────
    constexpr I64 kNumSims = 500;
    const auto sims = monte_carlo_comparison<MyFinances>(kBase, {
        fire_sale(25_USD),
        fire_sale(35_USD),
        fire_sale(50_USD),
    }, kOptimistic, kNumSims);
    xy_plot<I64, double>(sims, {
        .title   = "Monte Carlo: Retirement Probability by Exit Price",
        .y_title = "P(Retired by Year)"
    });

    return 0;
}
