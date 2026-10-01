#pragma once

#include <string>
#include <optional>

#include "canni/Macros.h"

namespace canni {

/// Zero-based enum for the month.
enum class Month : I64 {
    Jan = 0,
    Feb = 1,
    Mar = 2,
    Apr = 3,
    May = 4,
    Jun = 5,
    Jul = 6,
    Aug = 7,
    Sep = 8,
    Oct = 9,
    Nov = 10,
    Dec = 11
};

/**
 * @class Day
 * @brief Stores a date as a triple of (day, month, year).
 */
class Day {
  public:
    struct Distance {
        pure Distance operator/(I64 d) const;
        I64 days = 0;
        I64 months = 0;
        I64 years = 0;
    };

    static constexpr Distance days(I64 n) { return Distance{.days = n}; }
    static constexpr Distance months(I64 n) { return Distance{.months = n}; }
    static constexpr Distance years(I64 n) { return Distance{.years = n}; }

    static std::optional<Day> parse(std::string_view str, const std::optional<std::string>& format = std::nullopt);

    /// Returns the last day of [year].
    static Day end_of_year(const I64 year) { return Day(31, Month::Dec, year); }

    Day() = default;
    explicit Day(const std::string &str) : Day(*parse(str)) {}
    explicit Day(I64 day, Month month, I64 year);

    /// Returns the number of days between from [rhs] to this date.
    pure I64 operator-(const Day &rhs) const;

    /// Returns a date that is [duration] after this date.
    pure Day operator+(const Distance &duration) const;
    Day &operator+=(const Distance &duration) { *this = *this + duration; return *this; }

    /// Returns a date that is [duration] before this date.
    pure Day operator-(const Distance &duration) const;
    Day &operator-=(const Distance &duration) { *this = *this - duration; return *this; }

    /// Returns the day of the month
    pure I64 day() const;

    /// Returns the month as an enum
    pure Month month() const;

    /// Returns the year.
    pure I64 year() const;

    /// Returns the 1st of the next month.
    pure Day next_month() const;

    /// Returns the same day/month, one year later.
    pure Day next_year() const;

    pure auto operator<=>(const Day &rhs) const = default;
    pure bool operator==(const Day &rhs) const = default;

    pure std::string to_string(const std::optional<std::string>& format = std::nullopt) const;

  private:
    friend std::hash<Day>;
    I64 year_;  // Numeric year
    I64 month_; // Month (0 - 12)
    I64 day_;   // Day of the month
};

consteval Day::Distance operator""_days(const unsigned long long n) { return Day::days(n); }
consteval Day::Distance operator""_months(const unsigned long long n) { return Day::months(n); }
consteval Day::Distance operator""_years(const unsigned long long n) { return Day::years(n); }

inline std::ostream &operator<<(std::ostream &os, const Day &day) { return os << day.to_string(); }

} // namespace canni

template <>
struct std::hash<canni::Day> {
    size_t operator()(const canni::Day &date) const noexcept {
        return std::hash<I64>()((date.year_ << 9) + (date.day_ << 4) + date.month_);
    }
};
