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
    };

    explicit Account(std::string name, Interest interest)
        : name_(std::move(name)), interest_(interest) {}

    /// Parses an account from the given data at [balance] and [principal] columns.
    /// If [principal] is not specified, assumes that the principal and balance are the same.
    /// Sets balance_ and principal_ from the last valid row; keeps all rows in history_.
    explicit Account(const nvl::Tensor<2, std::string> &data, const std::string &name,
                     const Interest &interest, I64 balance, nvl::Maybe<I64> principal = nvl::None);

    Account(const Account &) = default;
    Account(Account &&) = delete;
    Account &operator=(const Account &) = delete;

    virtual ~Account() = default;

    // Current per-row state
    pure USD balance() const { return balance_; }
    pure USD principal() const { return principal_; }
    pure Entry entry() const { return {principal_, balance_}; }

    // Returns current balance. The Day argument is accepted for call-site compatibility but ignored.
    pure USD operator[](const Day &) const { return balance_; }

    // Historical lookup from CSV data. Returns the entry at or before [date], or {} if none.
    pure virtual Entry get(const Day &date) const;

    // Returns all dates present in the historical CSV data.
    pure virtual std::vector<Day> dates() const;

    // Advances balance_ in place from [from] to [to] using the interest rate.
    // Default implementation applies the interest model and calls tax_gain on the delta.
    virtual void advance(Taxes &taxes, const Day &from, const Day &to);

    // Creates a deep copy of this account for a Row snapshot.
    virtual std::shared_ptr<Account> clone() const = 0;

    // Modifies the current balance and principal. The Day argument is ignored.
    virtual void deposit(const Day &, const USD &amount) { balance_ += amount; principal_ += amount; }

    // Decreases current balance by up to [amount]. Returns the actual withdrawn.
    [[nodiscard]] virtual USD withdraw(Taxes &taxes, const Day &date, const USD &amount);

    // Tax hooks for subtype-specific accounting.
    virtual void tax_gain(Taxes &taxes, const Day &day, const USD &gain) const = 0;
    virtual void tax_sale(Taxes &taxes, const Day &day, const USD &gain, const USD &total) const = 0;

    pure std::string name() const { return name_; }
    pure std::string_view category() const { return _get_classtag().name; }
    void set_apy(const Interest interest) { interest_ = interest; }
    Interest apy() const { return interest_; }

    std::string name_;
    USD balance_;
    USD principal_;
    Interest interest_;
    std::map<Day, Entry> history_;  // CSV-loaded entries; read-only after construction
};

struct Bonds : Account {
    class_tag(Bonds, Account);
    using Account::Account;
    std::shared_ptr<Account> clone() const override { return std::make_shared<Bonds>(*this); }
    void tax_gain(Taxes &taxes, const Day &date, const USD &gain) const override {
        taxes.income(name_ + " Dividends", date, gain);
    }
    void tax_sale(Taxes &, const Day &, const USD &, const USD &) const override { }
};

struct Cash : Account {
    class_tag(Cash, Account);
    using Account::Account;
    std::shared_ptr<Account> clone() const override { return std::make_shared<Cash>(*this); }
    void tax_gain(Taxes &taxes, const Day &date, const USD &amount) const override {
        taxes.income(name_ + " Interest", date, amount);
    }
    void tax_sale(Taxes &, const Day &, const USD &, const USD &) const override { }
};

struct RealEstate : Account {
    class_tag(RealEstate, Account);
    using Account::Account;
    std::shared_ptr<Account> clone() const override { return std::make_shared<RealEstate>(*this); }
    void tax_gain(Taxes &, const Day &, const USD &) const override { }
    void tax_sale(Taxes &taxes, const Day &date, const USD &gain, const USD &) const override {
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

    std::shared_ptr<Account> clone() const override { return std::make_shared<Retirement>(*this); }

    void tax_gain(Taxes &, const Day &, const USD &) const override { }
    void tax_sale(Taxes &taxes, const Day &date, const USD &gain, const USD &) const override {
        return_if(kind_ == kPostTax);
        taxes.income("Retirement", date, gain);
    }

    Kind kind_;
};

struct Stocks : Account {
    class_tag(Stocks, Account);
    using Account::Account;
    std::shared_ptr<Account> clone() const override { return std::make_shared<Stocks>(*this); }
    void tax_gain(Taxes &taxes, const Day &date, const USD &gain) const override { taxes.dividends(date, gain * 0.1); }
    void tax_sale(Taxes &taxes, const Day &date, const USD &gain, const USD &) const override {
        taxes.long_term(date, gain * 0.9);
    }
};
