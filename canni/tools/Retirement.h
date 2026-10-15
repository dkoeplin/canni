#pragma once

#include "canni/Macros.h"
#include "canni/account/Portfolio.h"

namespace canni {

struct DayAndBalance {
    Day day;
    USD balance;
};

template <typename Func>
DayAndBalance bisect(const Func simulate, Day lo, Day hi, const Day::Distance step) {
    DayAndBalance best {.day = hi, .balance = 0_USD};
    while (lo <= hi) {
        const Day mid = lo + Day::days((hi - lo) / 2);
        if (const auto b = simulate(mid); b > 0_USD) {
            best = {mid, b};
            hi = mid - step;
        }
        else {
            lo = mid + step;
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

    const Day &start = params.base.today;
    DayAndBalance result = bisect(simulate, start, params.base.ending, 1_years);
    result = bisect(simulate, std::max(start, result.day - 1_years), result.day + 1_years, 1_months);
    result = bisect(simulate, std::max(start, result.day - 2_months), result.day + 2_months, 1_days);
    result.day = std::max(start, result.day);
    return result;
}

} // namespace canni
