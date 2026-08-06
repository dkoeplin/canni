#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <utility>

#include "nvl/data/List.h"
#include "nvl/data/Maybe.h"
#include "nvl/data/Tensor.h"

#include "invest/Day.h"
#include "invest/Interest.h"
#include "invest/Taxes.h"
#include "invest/USD.h"

abstract struct Account {
    class_tag(Account);
    struct Entry {
        USD principal;
        USD balance;
        bool projected = false;
    };

    explicit Account(std::string name, Interest interest) : name_(std::move(name)), interest_(interest) {}

    /// Parses an account from the given data at [balance] and [principal] columns.
    /// If [principal] is not specified, assumes that the principal and balance are the same.
    explicit Account(const nvl::Tensor<2, std::string> &data, const std::string &name,
                     const Interest &interest, I64 balance, nvl::Maybe<I64> principal = nvl::None);

    /// No moving or copying accounts. They should only be owned by the portfolio (Accounts) object.
    Account(const Account &rhs) = delete;
    Account(const Account &&rhs) = delete;
    Account &operator=(const Account &rhs) = delete;

    virtual ~Account() = default;

    pure virtual std::vector<Day> dates() const;

    /// Creates a projection at [date] if no explicit entry exists yet.
    /// Projected gains are registered for taxes through tax_gain.
    virtual void ensure(Taxes &taxes, const Day &date) const;

    /// Returns the entry registered to the closest day at or before [date].
    pure virtual Entry get(const Day &date) const;

    /// Returns the balance registered to the closest day at or before [date].
    pure USD operator[](const Day &date) const { return get(date).balance; }

    /// Increases the total balance and principal at [date] and all following registered dates.
    virtual void deposit(const Day &date, const USD &amount);

    /// Decreases the total balance at [date] and all following registered dates.
    /// Returns the actual amount withdrawn, which may be less than [amount].
    [[nodiscard]] virtual USD withdraw(Taxes &taxes, const Day &date, const USD &amount);

    /// Set the taxed amount when a projection computes a change in amount.
    virtual void tax_gain(Taxes &taxes, const Day &day, const USD &gain) const = 0;

    /// Set the taxed amount when a portion is withdrawn/sold.
    virtual void tax_sale(Taxes &taxes, const Day &day, const USD &gain, const USD &total) const = 0;

    /// Returns the name of this account.
    pure std::string name() const { return name_; }

    /// Returns the category of this account based on class tag.
    pure std::string_view category() const { return _get_classtag().name; }

    /// Sets the APY for this account. Existing memoized entries are not changed.
    void set_apy(const Interest interest) { interest_ = interest; }
    Interest apy() const { return interest_; }

    std::string name_;                    /// Name of the account
    mutable std::map<Day, Entry> value_;  /// Recorded account balances and projections, ordered by date
    Interest interest_;                   /// APY and interest type
};

struct Bonds : Account {
    class_tag(Bonds, Account);
    using Account::Account;
    // Dividends on bond accounts are standard income. Assume 100% of the gain is from taxed income here.
    // (This may not be strictly true, as some fluctuations due to price changes do occur)
    void tax_gain(Taxes &taxes, const Day &date, const USD &gain) const override {
        taxes.income(name_ + " Dividends", date, gain);
    }
    void tax_sale(Taxes &, const Day &, const USD &, const USD &) const override { }
};

struct Cash : Account {
    class_tag(Cash, Account);
    using Account::Account;
    // Interest on cash accounts is standard income. No taxes on cash withdrawal.
    void tax_gain(Taxes &taxes, const Day &date, const USD &amount) const override {
        taxes.income(name_ + " Interest", date, amount);
    }
    void tax_sale(Taxes &, const Day &, const USD &, const USD &) const override { }
};

struct RealEstate : Account {
    class_tag(RealEstate, Account);
    using Account::Account;
    // Gains for real estate are entirely unrealized until sale (except for corresponding property taxes).
    void tax_gain(Taxes &, const Day &, const USD &) const override { }
    void tax_sale(Taxes &taxes, const Day &date, const USD &gain, const USD &) const override {
        // Assume 500K married/joint exclusion for gains on primary housing (2 of last 5 years)
        static const auto kExclusion = 500000_USD;
        if (const auto remain = gain - kExclusion; remain > 0_USD)
            taxes.long_term(date, remain);
    }
};

struct Retirement : Account {
    class_tag(Retirement, Account);
    enum Kind { kPreTax, kPostTax };

    explicit Retirement(std::string name, Interest interest, Kind kind)
        : Account(std::move(name), interest), kind_(kind) {}
    explicit Retirement(const nvl::Tensor<2, std::string> &data, const std::string &name,
                        const Interest &interest, Kind kind, I64 balance, nvl::Maybe<I64> principal = nvl::None)
        : Account(data, name, interest, balance, principal), kind_(kind) {}

    // Gains within a retirement account are not taxed, including reinvested dividends.
    void tax_gain(Taxes &, const Day &, const USD &) const override { }

    // Gains on withdrawals from a retirement account are only taxed on pre-tax accounts.
    // Assume that gains are exclusively long term gains.
    void tax_sale(Taxes &taxes, const Day &date, const USD &gain, const USD &) const override {
        return_if(kind_ == kPostTax);
        taxes.income("Retirement", date, gain);
    }

    Kind kind_;
};

struct Stocks : Account {
    class_tag(Stocks, Account);
    using Account::Account;
    // Assume about 10% of gains are qualified dividends (usually reinvested)
    void tax_gain(Taxes &taxes, const Day &date, const USD &gain) const override { taxes.dividends(date, gain * 0.1); }
    // The remaining 90% of gains are not realized until sales. Assume they are exclusively long term gains.
    void tax_sale(Taxes &taxes, const Day &date, const USD &gain, const USD &) const override {
        taxes.long_term(date, gain * 0.9);
    }
};