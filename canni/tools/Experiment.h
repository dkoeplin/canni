#pragma once

#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include "canni/data/Day.h"
#include "canni/account/Portfolio.h"
#include "canni/plot/Plotting.h"
#include "canni/plot/Series.h"
#include "canni/tools/Retirement.h"

namespace canni {

template <typename X, typename Step = X>
struct Range {
    X min;
    X max;
    Step step;
};

/// Returns the values [min, min + step, ...] up to and including [max].
template <typename X, typename Step>
std::vector<X> values(const Range<X, Step> &range) {
    std::vector<X> result;
    for (X x = range.min; x <= range.max; x += range.step) {
        result.push_back(x);
    }
    return result;
}

/// A named family of scenarios, parameterized by the swept value.
template <typename X>
struct Variant {
    std::string name;
    std::function<Scenario(X)> mods;
};

/// Evaluates [metric] on each variant's scenario at each of [xs], returning one series per variant.
/// [metric] takes a Scenario and may return any type, e.g. a Day, USD, or struct of several results.
template <typename X, typename Metric, typename Y = std::invoke_result_t<Metric, const Scenario &>>
std::vector<Series<X, Y>> sweep(const std::vector<X> &xs, const std::vector<Variant<X>> &variants, const Metric &metric) {
    std::vector<Series<X, Y>> result;
    for (const auto &variant : variants) {
        Series<X, Y> &series = result.emplace_back(Series<X, Y>{.name = variant.name});
        for (const X &x : xs) {
            series.points.emplace_back(x, metric(variant.mods(x)));
        }
    }
    return result;
}

template <typename X>
struct RetirementSweep {
    std::vector<Series<X, Day>> retirement; // Earliest viable retirement date, per variant
    std::vector<Series<X, USD>> balance;    // Ending net worth (today's dollars) when retiring on that date
};

/// Sweeps the earliest viable retirement date and corresponding ending balance for each variant.
template <typename P, typename X>
    requires PortfolioSubclass<P>
RetirementSweep<X> sweep_retirement(const typename P::Params &params,
                                    const std::vector<X> &xs,
                                    const std::vector<Variant<X>> &variants) {
    const auto results = sweep(xs, variants, [&](const Scenario &s) { return min_retirement<P>(params, s); });
    RetirementSweep<X> out;
    for (const auto &series : results) {
        auto &days = out.retirement.emplace_back(Series<X, Day>{.name = series.name});
        auto &balances = out.balance.emplace_back(Series<X, USD>{.name = series.name});
        for (const auto &[x, result] : series.points) {
            days.points.emplace_back(x, result.day);
            balances.points.emplace_back(x, result.balance);
        }
    }
    return out;
}

/**
 * @struct Experiment
 * @brief Performs a sweep over the X range, recording the Y result at each step.
 */
template <typename P, typename X, typename XStep = X, typename Y = Day>
    requires PortfolioSubclass<P>
struct Experiment : XYPlotOptions {
    explicit Experiment(P::Params p) : params(p) {}
    Experiment(const Experiment &) = delete;
    Experiment(const Experiment &&) = delete;
    Experiment &operator=(const Experiment &) = delete;

    void run(const bool plot = true) {
        params.base.verbose = false;
        series.name = "Experiment"; // TODO: Configure based on Y or y_func
        X x = x_range.min;
        while (x <= x_range.max) {
            series.points.emplace_back(x, y_func(x));
            x += x_range.step;
        }
        if (plot) xy_plot<X, Y>({series}, *this);
    }

    P::Params params;                                                    // Portfolio settings, constant over sweep
    Range<X, XStep> x_range;                                             // The range of X values to sweep
    std::function<Scenario(X)> mods = [](const X){ return Scenario{}; }; // Scenario modifier for each value of sweep
    // TODO: Change to have no default implementation if Y != Day
    std::function<Y(X)> y_func =                                         // Function computed on each value of sweep
        [this](const X x){ return min_retirement<P>(params, mods(x)).day; };
    Series<X, Y> series;                                                 // Resulting dataset after sweep
};

} // namespace canni
