#pragma once

#include "canni/Macros.h"
#include "canni/account/Portfolio.h"

namespace canni {

template <typename Func>
Day bisect(const Func simulate, Day lo, Day hi, const Day::Distance step) {
    Day best = hi;
    while (lo <= hi) {
        const Day mid = lo + Day::days((hi - lo) / 2);
        if (simulate(mid)) {
            best = mid;
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
keep Day min_retirement_date(typename P::Params params, const Scenario &scenario) {
    params.base.verbose = false;
    const auto history = P(params, {}).snapshot(); // pre-load history once

    const auto simulate = [&](Day retire) -> bool {
        params.base.retire = retire;
        params.base.history = history;
        P portfolio(params, scenario);
        portfolio.project();
        return portfolio.solvent();
    };

    Day result = bisect(simulate, params.base.today, params.base.ending, 1_years);
    result = bisect(simulate, result - 1_years, result + 1_years, 1_months);
    // TODO: Make granularity of bisects selectable
    result = bisect(simulate, result - 2_months, result + 2_months, 1_days);
    return result;
}

} // namespace canni
