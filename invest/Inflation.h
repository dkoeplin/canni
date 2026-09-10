#pragma once

#include "invest/Interest.h"
#include "invest/USD.h"

struct Accounts;

class Inflation {
  public:
    explicit Inflation(const Accounts *parent, const Day &today, const double avg_inflation)
        : parent_(parent), kToday(today),
          kYearly(Interest::Yearly(avg_inflation)),
          kMonthly(Interest::Monthly(avg_inflation)) {}


    /// Returns the amount adjusted for cumulative inflation from today to [date].
    pure USD operator()(const Day &date, const USD &amount) const;

    pure USD operator()(const Day &start, const Day &end, const USD &amount) const;

    /// Returns the amount adjusted for inflation, compounding once per year.
    pure USD yearly(const Day &date, const USD &amount) const;

    /// Returns roughly the amount in today's dollars, reverse-adjusted for inflation.
    pure USD inverse(const Day &date, const USD &amount) const;

  private:
    /// Cumulative inflation multiplier from kToday to [date], compounding year by year.
    pure double cumulative(const Day &date) const;

    /// Cumulative inflation multiplier from [start] to [end], compounding year by year.
    pure double cumulative(const Day &start, const Day &end) const;

    const Accounts *parent_ = nullptr;
    const Day kToday;
    const Interest kYearly;
    const Interest kMonthly;
};
