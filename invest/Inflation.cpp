#include "Inflation.h"

#include "invest/account/Accounts.h"

USD Inflation::operator()(const Day &date, const USD &amount) const {
    return returns_ ? USD::round(amount.f64() * cumulative(date)) : kMonthly.estimate(kToday, date, amount);
}

USD Inflation::yearly(const Day &date, const USD &amount) const {
    return returns_ ? USD::round(amount.f64() * cumulative(date)) : kYearly.estimate(kToday, date, amount);
}

USD Inflation::inverse(const Day &date, const USD &amount) const {
    return returns_ ? USD::round(amount.f64() / cumulative(date)) : kMonthly.inverse(kToday, date, amount);
}

double Inflation::cumulative(const Day& date) const {
    return returns_->cumulative(AssetType::kInflation, date.year());
}
