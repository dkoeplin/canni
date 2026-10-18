#pragma once

#include <algorithm>

#include "canni/Macros.h"
#include "canni/account/Portfolio.h"

namespace canni {

struct DayAndBalance {
    Day day;
    USD balance;
};

/// Returns the first day in [lo, lo + step, lo + 2 * step, ..., hi] on which [simulate] returns a positive balance,
/// assuming that once a day passes, all later days do too. Returns {hi, 0} if no day passes.
template <typename Func>
DayAndBalance first_passing(const Func &simulate, const Day lo, const Day hi, const Day::Distance step) {
    const auto day = [&](const I64 k) {
        return std::min(hi, lo + Day::Distance{.days = step.days * k, .months = step.months * k, .years = step.years * k});
    };
    I64 n = 0;
    while (day(n) < hi) ++n;

    DayAndBalance best {.day = hi, .balance = 0_USD};
    I64 a = 0, b = n;
    while (a <= b) {
        const I64 k = a + (b - a) / 2;
        if (const auto balance = simulate(day(k)); balance > 0_USD) {
            best = {day(k), balance};
            b = k - 1;
        }
        else {
            a = k + 1;
        }
    }
    return best;
}

template <typename P>
    requires PortfolioSubclass<P>
keep DayAndBalance min_retirement(typename P::Params params, const Scenario &scenario) {
    params.base.verbose = false;
    const auto history = P(params, {}).snapshot(); // pre-load history once

    const auto simulate = [&](Day retire) -> USD {
        params.base.retire = retire;
        params.base.history = history;
        P portfolio(params, scenario);
        portfolio.project();
        return portfolio.inflation.inverse(portfolio.ending_day(), portfolio.net_worth());
    };

    // Narrow down by year, then month, then day. Each pass searches between the last failing
    // and first passing day of the previous pass.
    const Day &start = params.base.today;
    DayAndBalance result = first_passing(simulate, start, params.base.ending, 1_years);
    result = first_passing(simulate, std::max(start, result.day - 1_years), result.day, 1_months);
    result = first_passing(simulate, std::max(start, result.day - 1_months), result.day, 1_days);
    return result;
}

} // namespace canni
