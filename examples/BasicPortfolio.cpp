/// BasicPortfolio.cpp — minimal example of canni: salary, expenses, retirement accounts,
/// housing scenarios, and a comparison plot.
///
/// Replace the placeholder values with your own numbers to use this as a starting point.

#include "account/Scenario.h"
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

        // Market assumptions
        Interest avg_stocks_apy    = 7.0_pct;
        Interest avg_bonds_apy     = 4.5_pct;
        Interest avg_401k_apy      = 7.0_pct;
        Interest avg_roth_apy      = 7.0_pct;
        Interest avg_real_estate   = 3.5_pct;

        // Annual amounts (in today's dollars)
        USD salary              = 120000_USD;
        USD partner_salary      = 80000_USD;
        USD living_expenses     = 60000_USD;
        USD health_insurance    = 18000_USD; // post-retirement only
        I64 medical_age         = 65;
        USD medical_per_year    = 2500_USD;
    };

    explicit MyFinances(const Params &p, const Scenario &scenario = {})
        : Portfolio(p.base, scenario), params_(p) {

        stocks.set_apy(p.avg_stocks_apy);
        bonds.set_apy(p.avg_bonds_apy);
        f401k.set_apy(p.avg_401k_apy);
        roth.set_apy(p.avg_roth_apy);
        home.set_apy(p.avg_real_estate);

        if (!p.base.history) {
            // Load historical balances from CSV if available.
            // import_csv("../data/tracking.csv", { Balance(checking), Balance(savings), ... });
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

                // Your salary (post-401K contribution)
                auto income = inflation.yearly(d, params_.salary) / 12 - contribution;
                const auto ss    = income * 0.062;
                const auto mc    = income * 0.0235;
                const auto with  = income * 0.22;
                (void)f401k.deposit(contribution);
                taxes.income(d, income);  taxes.social_security(d, ss);
                taxes.withholding(d, with); taxes.medicare(d, mc);
                deposit(income - with - mc - ss);

                // Partner's salary
                const auto partner = inflation.yearly(d, params_.partner_salary) / 12;
                const auto pmc  = partner * 0.0235;
                const auto pwth = partner * 0.15;
                taxes.income(d, partner);
                taxes.withholding(d, pwth); taxes.medicare(d, pmc);
                deposit(partner - pwth - pmc);
            }

            (void)withdraw("Living expenses", inflation(d, params_.living_expenses / 12));

            if (d >= params_.base.retire)
                (void)withdraw("Health insurance", inflation(d, params_.health_insurance / 12));

            const I64 age = d.year() - params_.base.birth.year();
            if (age > params_.medical_age)
                (void)withdraw("Medical", inflation(d, params_.medical_per_year / 12));
        });
    }

    bool solvent() const override {
        // Liquid assets only — exclude illiquid real estate from solvency check.
        return total() - get<RealEstate>().balance() > 0_USD;
    }

    void deposit(const USD amount) const override { (void)checking.deposit(amount); }

    void invest() const override {
        // Sweep excess cash (beyond a $30K emergency fund) into the stock portfolio.
        const auto excess = checking.balance() - inflation(current_day(), 30000_USD);
        if (excess > 0_USD)
            (void)stocks.deposit(checking.withdraw(excess));
    }

    // Accounts
    Cash       &checking = add<Cash>("Checking",        0.0_pct);
    Cash       &savings  = add<Cash>("High-Yield Savings", 4.5_pct);
    Stocks     &stocks   = add<Stocks>("Index Funds",   7.0_pct);
    Bonds      &bonds    = add<Bonds>("Bonds",          4.5_pct);
    Retirement &f401k    = add<Retirement>("401(k)",    7.0_pct, Retirement::kPreTax);
    Retirement &roth     = add<Retirement>("Roth IRA",  7.0_pct, Retirement::kPostTax);
    RealEstate &home     = add<RealEstate>("Home",      3.5_pct);

    const Params params_;
};

// ────────────────────────────────────────────────────────────
// Scenario modifiers
// ────────────────────────────────────────────────────────────

using Mod = TargetedScenario<MyFinances>;

// Buy a home outright for [price] on [when].
Mod buy_home(Day when, USD price) {
    return {"Buy Home", [when, price](MyFinances &s) {
        s.add<Once>("Buy Home", when, [&s, price](Day d) {
            const USD cost = s.params_.avg_real_estate.estimate(s.params_.base.today, d, price);
            (void)s.home.deposit(cost);
            (void)s.withdraw("Home purchase", cost * 1.03); // + 3% closing costs
        });
        s.add<Monthly>("Home Expenses", when, [&s](Day d) {
            const auto base = s.home.balance() * 0.8;
            return_if(base <= 0_USD);
            (void)s.withdraw("Upkeep + tax", (base * 0.02) / 12);
            s.taxes.property_tax(d, (base * 0.01) / 12);
        });
    }};
}

// Continue renting at [monthly_rent] instead of buying.
Mod rent(USD monthly_rent) {
    return {"Rent", [monthly_rent](MyFinances &s) {
        s.add<Monthly>("Rent", s.params_.base.today, [&s, monthly_rent](Day d) {
            (void)s.withdraw("Rent", s.inflation(d, monthly_rent));
        });
    }};
}

// Convert [pct] of 401(k) to Roth each year after retirement.
Mod roth_conversion(double pct) {
    return {"Roth Conversion", [pct](MyFinances &s) {
        s.add<Yearly>("Roth Conversion", s.params_.base.retire + 1_years, [&s, pct](Day) {
            const USD amount = s.f401k.withdraw(s.f401k.balance() * pct);
            (void)s.roth.deposit(amount);
        });
    }};
}

} // namespace

int main() {
    const Day kBirth("01/15/1990");
    const Day kToday("01/01/2026");

    const MyFinances::Params kBase {
        .base = {
            .avg_inflation_rate = 3.0_pct,
            .birth              = kBirth,
            .today              = kToday,
            .retire             = kToday + 10_years,
            .ending             = kBirth + 90_years,
            .verbose            = true,
        },
        .salary           = 120000_USD,
        .partner_salary   = 80000_USD,
        .living_expenses  = 60000_USD,
        .health_insurance = 18000_USD,
    };

    // ── Single verbose run ──────────────────────────────────
    MyFinances base(kBase, rent(3000_USD) | roth_conversion(0.10));
    base.project();

    // ── Housing comparison ──────────────────────────────────
    compare<MyFinances>(kBase, {
        rent(3000_USD),
        buy_home(kToday + 6_months, 600000_USD),
        buy_home(kToday + 6_months, 800000_USD),
        buy_home(kToday + 6_months, 1000000_USD),
    }, {
        .title   = "Rent vs. Buy Comparison",
        .y_title = "Projected Balance (Today's Dollars)"
    });

    // ── Monte Carlo ─────────────────────────────────────────
    constexpr I64 kNumSims = 500;
    const auto sims = monte_carlo_comparison<MyFinances>(kBase, {
        rent(3000_USD),
        buy_home(kToday + 6_months, 600000_USD),
        buy_home(kToday + 6_months, 800000_USD),
    }, kConservative, kNumSims);
    xy_plot<I64, double>(sims, {
        .title   = "Monte Carlo: Retirement Probability",
        .y_title = "P(Retired by Year)"
    });

    return 0;
}
