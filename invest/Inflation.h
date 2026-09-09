#pragma once

#include "invest/Interest.h"
#include "invest/USD.h"

struct MarketReturns;

class Inflation {
  public:
    explicit Inflation(const Day &today, const double avg_inflation)
        : kToday(today),
          kYearly(Interest::Yearly(avg_inflation)),
          kMonthly(Interest::Monthly(avg_inflation)) {}

    /// Switches to per-year inflation rates instead of the fixed average.
    /// [returns] must outlive this object.
    void set_rates(const MarketReturns &returns) { returns_ = &returns; }

    /// Returns the amount adjusted for cumulative inflation from today to [date].
    pure USD operator()(const Day &date, const USD &amount) const;

    /// Returns the amount adjusted for inflation, compounding once per year.
    pure USD yearly(const Day &date, const USD &amount) const;

    /// Returns roughly the amount in today's dollars, reverse-adjusted for inflation.
    pure USD inverse(const Day &date, const USD &amount) const;

  private:
    /// Cumulative inflation multiplier from kToday to [date], compounding year by year.
    pure double cumulative(const Day &date) const;

    const Day kToday;
    const Interest kYearly;
    const Interest kMonthly;
    const MarketReturns *returns_ = nullptr;
};
