#pragma once

#include "canni/tools/Retirement.h"

namespace canni {

/// Runs and plots multiple modifier scenarios against the same base params.
/// Prints the minimum retirement date for each.
template <typename P>
    requires PortfolioSubclass<P>
void compare(typename P::Params params, const std::vector<Scenario> &mods, const XYPlotOptions& opts = {}) {
    params.base.verbose = false;
    const auto history = P(params, {}).totals("History");
    std::vector<Series<Day, USD>> series = {history};

    for (const auto &mod : mods) {
        P s(params, mod);
        series.push_back(s.project_until(params.base.ending).totals(mod.name));
        std::cout << "Min retirement year (" << mod.name << "): " << min_retirement_date<P>(params, mod) << "\n";
    }
    xy_plot(series, opts);
}

} // namespace canni
