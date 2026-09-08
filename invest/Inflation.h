#pragma once

#include <vector>

#include "invest/Interest.h"
#include "invest/USD.h"

class Inflation {
  public:
    explicit Inflation(const Day &today, const double avg_inflation)
        : kToday(today),
          kYearly(Interest::Yearly(avg_inflation)),
          kMonthly(Interest::Monthly(avg_inflation)) {}

    /// Switches to per-year inflation rates instead of the fixed average.
    /// [rates] must outlive this object. [base_year] is the year of rates[0].
    void set_rates(const std::vector<double> &rates, I64 base_year) {
        rates_ = &rates;
        base_year_ = base_year;
    }

    /// Returns the amount adjusted for cumulative inflation from today to [date].
    pure USD operator()(const Day &date, const USD &amount) const {
        if (rates_) return USD::round(amount.f64() * cumulative(date));
        return kMonthly.estimate(kToday, date, amount);
    }

    /// Returns the amount adjusted for inflation, compounding once per year.
    pure USD yearly(const Day &date, const USD &amount) const {
        if (rates_) return USD::round(amount.f64() * cumulative(date));
        return kYearly.estimate(kToday, date, amount);
    }

    /// Returns roughly the amount in today's dollars, reverse-adjusted for inflation.
    pure USD inverse(const Day &date, const USD &amount) const {
        if (rates_) return USD::round(amount.f64() / cumulative(date));
        return kMonthly.inverse(kToday, date, amount);
    }

  private:
    /// Cumulative inflation multiplier from kToday to [date], compounding year by year.
    pure double cumulative(const Day &date) const {
        double m = 1.0;
        for (I64 y = kToday.year(); y < date.year(); ++y) {
            const I64 idx = y - base_year_;
            if (idx >= 0 && idx < static_cast<I64>(rates_->size()))
                m *= 1.0 + (*rates_)[idx];
        }
        return m;
    }

    const Day kToday;
    const Interest kYearly;
    const Interest kMonthly;
    const std::vector<double> *rates_ = nullptr;
    I64 base_year_ = 0;
};
