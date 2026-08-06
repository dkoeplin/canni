#pragma once

#include "invest/Interest.h"
#include "invest/USD.h"

class Inflation {
  public:
    explicit Inflation(const Day &today, const double avg_inflation)
        : kToday(today),
          kYearly(Interest::Yearly(avg_inflation)),
          kMonthly(Interest::Monthly(avg_inflation)) {}

    /// Returns the amount adjusted for inflation, compounding once per month.
    pure USD operator()(const Day &date, const USD &amount) const { return kMonthly.estimate(kToday, date, amount); }

    /// Returns the amount adjusted for inflation, compounding once per year.
    pure USD yearly(const Day &date, const USD &amount) const { return kYearly.estimate(kToday, date, amount); }

    /// Returns roughly the amount in today's dollars, reverse-adjusted for inflation.
    pure USD inverse(const Day &date, const USD &amount) const { return kMonthly.inverse(kToday, date, amount); }

  private:
    const Day kToday;
    const Interest kYearly;
    const Interest kMonthly;
};



