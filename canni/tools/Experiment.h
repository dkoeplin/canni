#pragma once

#include <functional>

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

/**
 * @struct Experiment
 * @brief Performs a sweep over the X range, recording the Y result at each step.
 */
template <typename P, typename X, typename XStep = X, typename Y = Day>
    requires PortfolioSubclass<P>
struct Experiment : XYPlotOptions {
    void run(const bool plot = true) {
        ASSERT(params.has_value(), "Experiment params was not yet set");
        params->base.verbose = false;
        series.name = "Min Retirement"; // TODO: Configure based on Y or y_func
        X x = x_range.min;
        while (x <= x_range.max) {
            series.points.emplace_back(x, y_func(x));
            x += x_range.step;
        }
        if (plot) xy_plot<X, Y>({series}, *this);
    }

    // TODO: Not actually optional, should be marked as "required" but uninitialized
    std::optional<typename P::Params> params;                            // Portfolio settings, constant over sweep
    Range<X, XStep> x_range;                                             // The range of X values to sweep
    std::function<Scenario(X)> mods = [](const X){ return Scenario{}; }; // Scenario modifier for each value of sweep
    // TODO: Change to have no default implementation if Y != Day
    std::function<Y(X)> y_func =                                         // Function computed on each value of sweep
        [this](const X x){ return min_retirement_date<P>(*params, mods(x)); };
    Series<X, Y> series;                                                 // Resulting dataset after sweep
};

} // namespace canni
