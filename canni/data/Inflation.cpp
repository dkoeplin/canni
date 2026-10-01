#include "Inflation.h"

#include "canni/account/Portfolio.h"

namespace canni {

USD Inflation::operator()(const Day &date, const USD &amount) const {
    return parent_->market_returns() ? USD::floor(amount.f64() * cumulative(date))
                                     : kMonthly.estimate(kToday, date, amount);
}

USD Inflation::operator()(const Day &start, const Day &end, const USD &amount) const {
    return parent_->market_returns() ? USD::floor(amount.f64() * cumulative(start, end))
                                     : kMonthly.estimate(start, end, amount);
}

USD Inflation::yearly(const Day &date, const USD &amount) const {
    return parent_->market_returns() ? USD::floor(amount.f64() * cumulative(date))
                                     : kYearly.estimate(kToday, date, amount);
}

USD Inflation::inverse(const Day &date, const USD &amount) const {
    return parent_->market_returns() ? USD::floor(amount.f64() / cumulative(date))
                                     : kMonthly.inverse(kToday, date, amount);
}

double Inflation::cumulative(const Day& date) const {
    return parent_->market_returns()->cumulative(Account::kInflation, date.year());
}

double Inflation::cumulative(const Day &start, const Day &end) const { return cumulative(end) / cumulative(start); }

} // namespace canni
