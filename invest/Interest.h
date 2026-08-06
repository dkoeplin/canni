#pragma once

#include "nvl/macros/Aliases.h"

#include "invest/Day.h"
#include "invest/USD.h"

class Interest {
  public:
    enum Type {
        kSimple,           // No compounding
        kCompoundsMonthly, // Compounding interest on the first of each month
        kCompoundsYearly,  // Compounding interest on the first day of each year
    };
    static Interest Simple(const F64 apy) { return Interest(kSimple, apy); }
    static Interest Monthly(const F64 apy) { return Interest(kCompoundsMonthly, apy); }
    static Interest Yearly(const F64 apy) { return Interest(kCompoundsYearly, apy); }

    explicit Interest(const Type type, const F64 apy) : type_(type), apy_(apy) {}

    pure USD estimate(const Day& a, const Day& b, const USD& initial) const;

    pure USD inverse(const Day &a, const Day &b, const USD &amount) const;

    /// Returns the expected annual percent yield (APY) for this account.
    pure F64 apy() const { return apy_; }
    pure Type type() const { return type_; }
    pure bool compounds() const { return type_ != kSimple; }

    pure auto operator<=>(const Interest&) const = default;
    pure bool operator==(const Interest&) const = default;

  private:
    /// Returns the number of times compounded between [a] (exclusive) and [b] (inclusive).
    /// Assumes that compounding happens on the first day of the period.
    pure I64 times_compounded(Day a, Day b) const;

    pure F64 rate_per_period() const;

    Type type_ = kSimple;
    F64 apy_ = 0.0;
};

constexpr Interest operator""_pct(const unsigned long long n) { return Interest::Monthly(static_cast<F64>(n)/100.0); }
constexpr Interest operator""_pct(const long double n) { return Interest::Monthly(n/100); }
