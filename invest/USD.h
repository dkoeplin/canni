#pragma once

#include <cmath>
#include <string>

#include "nvl/macros/Aliases.h"
#include "nvl/macros/Pure.h"

/**
 * @class USD
 * @brief Wrapper representing US dollars in a fixed point format with exactly two decimal points.
 */
class USD {
public:
    static constexpr USD dollars(const I64 dollars) { return USD(dollars * 100); }
    static constexpr USD round(const F64 dollars) { return USD(static_cast<I64>(dollars * 100)); }
    static USD parse(const std::string &str);

    USD() = default;
    USD(const USD &rhs) noexcept = default;
    USD(const USD &&rhs) noexcept : cents_(rhs.cents_) {}
    USD &operator=(const USD &rhs) noexcept = default;

    pure USD operator-() const { return USD(-cents_); }
    pure USD operator*(const double rhs) const { return round((cents_ * rhs) / 100.0); }
    pure USD operator/(const double rhs) const { return round((cents_ / rhs) / 100.0); }
    pure USD operator*(const int rhs) const { return USD(cents_ * rhs); }
    pure USD operator/(const int rhs) const { return USD(cents_ / rhs); }
    pure USD operator*(const I64 rhs) const { return USD(cents_ * rhs); }
    pure USD operator/(const I64 rhs) const { return USD(cents_ / rhs); }
    pure USD operator+(const USD &rhs) const { return USD(cents_ + rhs.cents_); }
    pure USD operator-(const USD &rhs) const { return USD(cents_ - rhs.cents_); }
    USD &operator+=(const USD &rhs) { cents_ += rhs.cents_; return *this; }
    USD &operator-=(const USD &rhs) { cents_ -= rhs.cents_; return *this; }

    auto operator<=>(const USD &) const = default;
    bool operator==(const USD &) const = default;

    pure std::string to_string() const;

    pure F64 f64() const { return static_cast<F64>(cents_) / 100; }

private:
    explicit constexpr USD(const I64 cents) : cents_(cents) {}
    I64 cents_ = 0;
};

inline std::ostream &operator<<(std::ostream &os, const USD &usd) { return os << usd.to_string(); }

consteval USD operator""_USD(const unsigned long long n) { return USD::dollars(n); }
consteval USD operator""_USD(const long double n) { return USD::round(n); }
