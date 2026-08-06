#include "Interest.h"

#include <iostream>

#include "nvl/macros/Unreachable.h"

#include "invest/Day.h"

USD Interest::estimate(const Day& a, const Day& b, const USD& initial) const {
    if (compounds()) {
        const I64 n = times_compounded(a, b);
        return USD::round(initial.f64() * std::pow(1 + rate_per_period(), n));
    }
    const F64 rate = apy_ / 365.0;                        // Rough estimate of daily percent yield
    const F64 time = static_cast<F64>(b - a);             // Elapsed days
    return USD::round(initial.f64() * (1 + rate * time)); // Estimated interest over principal
}

pure USD Interest::inverse(const Day &a, const Day &b, const USD &amount) const {
    if (compounds()) {
        const I64 n = times_compounded(a, b);
        return USD::round(amount.f64() / std::pow(1 + rate_per_period(), n));
    }
    const F64 rate = apy_ / 365.0;                        // Rough estimate of daily percent yield
    const F64 time = static_cast<F64>(b - a);             // Elapsed days
    return USD::round(amount.f64() / (1 + rate * time));
}


I64 Interest::times_compounded(Day a, Day b) const {
    switch (type_) {
    case kCompoundsYearly: {
        const I64 year_a = a.year();
        const I64 year_b = b.year();
        return std::max<I64>(0, year_b - year_a);
    }
    case kCompoundsMonthly: {
        // ASSUMPTION: Compounding happens on the first of the month.
        const I64 months_a = a.year() * 12 + static_cast<I64>(a.month());
        const I64 months_b = b.year() * 12 + static_cast<I64>(b.month());
        return std::max<I64>(0, months_b - months_a);
    }
    case kSimple:
        return 0;
    }
    UNREACHABLE;
}

F64 Interest::rate_per_period() const {
    switch (type_) {
    case kCompoundsYearly:
        return apy_;
    case kCompoundsMonthly:
        return std::pow(1 + apy_, 1.0/12.0) - 1;
    case kSimple:
        return apy_;
    }
    UNREACHABLE;
}