#pragma once

#include <random>
#include <thread>
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

    params.base.verbose = false;
    params.base.history = P(params, {}).snapshot();

    const I64 n_threads = std::max(I64{1}, static_cast<I64>(std::thread::hardware_concurrency()));

    std::vector<Series<I64, double>> results;
    for (const auto &scenario : scenarios) {
        Series<I64, double> result;
        result.name = scenario.name;
        for (I64 y = base_year; y <= end_year; ++y) result.points.emplace_back(y, 0);

        std::vector<I64> retire_years(n_sims);
        {
            std::vector<std::thread> threads;
            threads.reserve(n_threads);
            for (I64 t = 0; t < n_threads; ++t) {
                threads.emplace_back([&, t]() {
                    std::mt19937 rng(std::random_device{}());
                    for (I64 i = t; i < n_sims; i += n_threads) {
                        auto sim_params = params;
                        sim_params.base.returns = MarketReturns(base_year, years, returns, rng);
                        retire_years[i] = min_retirement_date<P>(sim_params, scenario).year();
                    }
                });
            }
            for (auto &th : threads) th.join();
        }

        for (const I64 retire_year : retire_years)
            for (I64 y = retire_year; y <= end_year; ++y)
                result.points[y - base_year].second += 1;
        for (auto &val : result.points | std::views::values) val /= static_cast<double>(n_sims);
        results.push_back(result);
    }
    return results;
}

} // namespace canni