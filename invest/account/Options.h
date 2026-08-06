#pragma once

#include <map>
#include <string>
#include <vector>

#include "invest/Day.h"
#include "invest/USD.h"
#include "invest/account/Account.h"
#include "invest/account/Company.h"

struct Options : Account {
    class_tag(Options, Account);

    enum Type { kNSO, kISO, kMix };
    struct Count {
        auto operator<=>(const Count &rhs) const = default;
        bool operator==(const Count &rhs) const = default;
        Count operator+(const Count &rhs) const { Count result = *this; return result += rhs; }
        Count operator-(const Count &rhs) const { Count result = *this; return result -= rhs; }
        Count &operator+=(const Count &rhs) {
            iso += rhs.iso;
            nso += rhs.nso;
            return *this;
        }
        Count &operator-=(const Count &rhs) {
            iso -= rhs.iso;
            nso -= rhs.nso;
            return *this;
        }

        I64 iso = 0;
        I64 nso = 0;
    };

    /// Parses a vesting schedule CSV with the expected format:
    ///   Pure grant  (5 cols): row, date, new_vested, cumulative, exercised
    ///   Mixed grant (7 cols): row, date, new_vested, ISO_vested, NSO_vested, cumulative, exercised
    explicit Options(Company company, const std::string &name, Type type, USD strike, const std::string &csv_path = "");

    pure Type type() const { return type_; }
    pure USD strike() const { return strike_; }

    void print() const;

    pure std::vector<Day> dates() const override;

    /// Cannot deposit into Options.
    void deposit(const Day &, const USD &) override { }

    /// Cannot withdraw from Options - use sell.
    USD withdraw(Taxes &, const Day &, const USD &) override { return 0.00_USD; }

    Options &vesting(const Day &day, Count count);

    /// Sells a specified number of options, assuming same-day (or short term) sales.
    /// Returns the total proceeds from the sale AND the exercise costs.
    struct [[nodiscard]] Sale {
        Sale &operator +=(const Sale &rhs) {
            exercise += rhs.exercise;
            proceeds += rhs.proceeds;
            return *this;
        }
        USD exercise;
        USD proceeds;
    };
    Sale sell(Taxes &taxes, const Day &day, const USD &price, Count count, bool cashless);

    /// Mark a specific number of options as previously sold at a certain date.
    Options &with_sale(Taxes &taxes, const Day &day, const USD &price, Count count, bool cashless = false) {
        (void)sell(taxes, day, price, count, cashless);
        return *this;
    }

    /// Returns the number of total vested shares on [day].
    pure Count vested(const Day &day) const;

    /// Returns the number of total sold shares on [day].
    pure Count sold(const Day &day) const;

    /// Returns the potential gain (market - strike) * (vested - sold) on [day].
    pure Entry get(const Day &day) const override;

    void set_exp(const Day &day) { expires_ = day; }

    /// Unrealized "gains" on options are not taxable.
    void tax_gain(Taxes &, const Day &, const USD &) const override { }

    /// Set the taxed amount when a portion is withdrawn/sold.
    void tax_sale(Taxes &, const Day &, const USD &, const USD &) const override {
        // Handled directly in `sell` for now
    }

  private:
    Company company_;               /// Reference to company
    Type type_;                     /// Option type (ISO, NSO, mixed)
    USD strike_;                    /// Strike price
    nvl::Maybe<Day> expires_;       /// Expiration date of options
    std::map<Day, Count> vested_;   /// Date -> cumulative vested count
    std::map<Day, Count> sold_;     /// Date -> cumulative sold
};

inline std::ostream &operator<<(std::ostream &os, const Options::Count &count) {
    return os << "{iso: " << count.iso << ", nso: " << count.nso << "}";
}