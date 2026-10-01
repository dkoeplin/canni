#pragma once

#include <vector>

#include "canni/account/MarketReturns.h"
#include "canni/account/Portfolio.h"
#include "canni/tools/Retirement.h"

namespace canni {

/// Example market performance distributions.

inline const std::vector<MarketReturns::AssetClass> kConservative {
    {.type = Account::kStocks,     .mean = 0.07,  .sigma = 0.15},
    {.type = Account::kBonds,      .mean = 0.045, .sigma = 0.06},
    {.type = Account::kCash,       .mean = 0.035, .sigma = 0.01},
    {.type = Account::kRealEstate, .mean = 0.035, .sigma = 0.10},
    {.type = Account::kInflation,  .mean = 0.035, .sigma = 0.015}};

inline const std::vector<MarketReturns::AssetClass> kOptimistic {
    {.type = Account::kStocks,     .mean = 0.09,  .sigma = 0.20},
    {.type = Account::kBonds,      .mean = 0.045, .sigma = 0.06},
    {.type = Account::kCash,       .mean = 0.035, .sigma = 0.01},
    {.type = Account::kRealEstate, .mean = 0.035, .sigma = 0.10},
    {.type = Account::kInflation,  .mean = 0.035, .sigma = 0.015}};

/// Runs a Monte-Carlo simulation [n_sims] times, pulling market conditions from the [returns] market distributions.
template <typename P>
    requires PortfolioSubclass<P>
keep std::vector<Series<I64, double>>
monte_carlo_comparison(typename P::Params params, const std::vector<Scenario> &scenarios,
                       const std::vector<MarketReturns::AssetClass> &returns,
                       const I64 n_sims = 500) {
    const I64 base_year = params.base.today.year();
    const I64 end_year  = params.base.ending.year();
    const I64 years     = end_year - base_year + 1;

    std::mt19937 rng(std::random_device{}());
    params.base.verbose = false;
    params.base.history = P(params, {}).snapshot();

    std::vector<Series<I64, double>> results;
    for (const auto &scenario : scenarios) {
        Series<I64, double> result;
        result.name = scenario.name;
        for (I64 y = base_year; y <= end_year; ++y) result.points.emplace_back(y, 0);

        for (I64 i = 0; i < n_sims; ++i) {
            params.base.returns = MarketReturns(base_year, years, returns, rng);
            const Day retire = min_retirement_date<P>(params, scenario);
            std::cout << "#" << i << ": " << retire << " {"
                << "stocks: " << params.base.returns->cumulative(Account::kStocks, end_year) << ", "
                << "bonds: " << params.base.returns->cumulative(Account::kBonds, end_year) << ", "
                << "cash: " << params.base.returns->cumulative(Account::kCash, end_year) << ", "
                << "inflation: " << params.base.returns->cumulative(Account::kInflation, end_year)
                << "}\n";
            for (I64 y = retire.year(); y <= end_year; ++y)
                result.points[y - base_year].second += 1;
        }
        for (auto &val : result.points | std::views::values) val /= static_cast<double>(n_sims);
        results.push_back(result);
    }
    return results;
}

} // namespace canni