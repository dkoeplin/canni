#pragma once

#include <map>
#include <optional>
#include <string>

#include "canni/account/Account.h"
#include "canni/account/Company.h"
#include "canni/data/Day.h"
#include "canni/data/USD.h"

namespace canni {

struct OptionsCount {
    auto operator<=>(const OptionsCount &) const = default;
    OptionsCount operator+(const OptionsCount &rhs) const { OptionsCount r = *this; return r += rhs; }
    OptionsCount operator-(const OptionsCount &rhs) const { OptionsCount r = *this; return r -= rhs; }
    OptionsCount &operator+=(const OptionsCount &rhs) { iso += rhs.iso; nso += rhs.nso; return *this; }
    OptionsCount &operator-=(const OptionsCount &rhs) { iso -= rhs.iso; nso -= rhs.nso; return *this; }
    pure I64 total() const { return iso + nso; }
    I64 iso = 0;
    I64 nso = 0;
};

struct OptionsSnapshot : AccountSnapshot {
    std::map<Day, OptionsCount> vested_;
    std::map<Day, OptionsCount> sold_;
};

struct Options : Account, OptionsSnapshot {
    using Count = OptionsCount;
    enum Type { kNSO, kISO, kMix };

    /// The results of a sale. Factors in that a sale may have an upfront cost which must be paid first.
    struct [[nodiscard]] Sale {
        Sale &operator +=(const Sale &rhs) {
            costs += rhs.costs;
            proceeds += rhs.proceeds;
            return *this;
        }
        USD costs;
        USD proceeds;
    };

    explicit Options(Portfolio *parent, Company company, const std::string &name, Type type, USD strike);

    /// Parses a vesting schedule CSV and populates the vesting schedule.
    ///   Pure grant  (5 cols): row, date, new_vested, cumulative, exercised
    ///   Mixed grant (7 cols): row, date, new_vested, ISO_vested, NSO_vested, cumulative, exercised
    void load(const std::string &csv_path);

    static constexpr Kind kKind = kOptions;
    pure Kind kind() const override { return kOptions; }
    pure Type type() const { return type_; }
    pure USD strike() const { return strike_; }

    /// Can't directly deposit or withdraw on Options - need to use an explicit sale instead.
    pure USD withdraw(const USD &) const override { return 0.00_USD; }
    pure USD deposit(const USD &) const override { return 0.00_USD; }
    pure Entry project(const DatedEntry &prev, const Day &next) const override;
    pure Entry estimate(const Day &day) const override;

    /// Sell a specified number of options at a certain price.
    /// Assumes a same-day sale, so spread is standard income and sale gains are short-term capital gains
    Sale sell(Day day, const USD &price, Count count, bool cashless = false);

    /// Mark a specific number of options as previously sold at a certain date.
    Options &with_sale(Day day, const USD &price, Count count, bool cashless = false);

    /// Returns the number of total vested shares on [day].
    pure Count vested(const Day &day) const;

    /// Returns the number of total sold shares on [day].
    pure Count sold(const Day &day) const;

    /// Returns the number of available (vested - sold) shares on [day].
    pure Count avail(const Day &day) const { return vested(day) - sold(day); }

    /// Sets the expiration of these options to [day].
    void set_exp(const Day &day) { expires_ = day; }

    void tax_gain(const Day &, const USD &) const override { /* Unrealized gains on Options are not taxable. */ }
    void tax_sale(const Day &, const USD &) const override { /* Handled directly in `sell` for now. */ }

    std::shared_ptr<AccountSnapshot> save() const override {
        return std::make_shared<OptionsSnapshot>(*this);
    }
    void restore(const AccountSnapshot &other) override {
        const auto &rhs = static_cast<const OptionsSnapshot &>(other);
        vested_ = rhs.vested_;
        sold_   = rhs.sold_;
    }

  private:
    Company company_;            /// Reference to company
    Type type_;                  /// Option type (ISO, NSO, mixed)
    USD strike_;                 /// Strike price
    std::optional<Day> expires_; /// Expiration date of options
};

inline std::ostream &operator<<(std::ostream &os, const Options::Count &count) {
    return os << "{iso: " << count.iso << ", nso: " << count.nso << "}";
}

} // namespace canni