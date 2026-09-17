#pragma once

#include <algorithm>
#include <functional>
#include <ranges>
#include <utility>

#include "nvl/data/List.h"
#include "nvl/data/Tensor.h"

#include "invest/Day.h"
#include "invest/Interest.h"
#include "invest/USD.h"

struct Accounts;

struct Entry {
    USD principal;
    USD balance;
};

struct DatedEntry : Entry {
    explicit DatedEntry(const Day &day, const Entry &entry) : Entry(entry), day(day) {}
    Day day;
};

abstract struct Account {
    class_tag(Account);

    explicit Account(Accounts *parent, std::string name) : parent_(parent), name_(std::move(name)) {}
    virtual ~Account() = default;

    /// Returns the latest balance of this account.
    pure USD balance() const;

    /// Deposits the specified amount into the latest balance of this account.
    /// Returns the actual amount deposited, which may be smaller than [amount].
    pure virtual USD deposit(const USD &amount) const;

    /// Withdraws [amount] from the latest balance of this account, incurring related taxes.
    /// Returns the actual amount withdrawn, which may be smaller than [amount].
    pure virtual USD withdraw(const USD &amount) const = 0;

    /// Projects the next balance based on the balance in [prev] and the time between [next] and [prev].
    pure virtual Entry project(const DatedEntry &prev, const Day &next) const = 0;

    /// Estimates an historical balance for this account on the given day.
    pure virtual Entry estimate(const Day &) const { return {}; }

    /// Taxes incurred during a "gain" (usually a projected increase).
    virtual void tax_gain(const Day &day, const USD &gain) const = 0;

    /// Taxes incurred during a "sale" (usually a withdrawal).
    virtual void tax_sale(const Day &day, const USD &gain) const = 0;

    pure std::string_view category() const { return _get_classtag().name; }
    pure const std::string &name() const { return name_; }

    void set_index(const U64 index) { index_ = index; }
    pure U64 index() const { return index_; }

  protected:
    Accounts *parent_;
    std::string name_;
    U64 index_ = UINT64_MAX;
};

enum class AssetType : U64 {
    kStocks,
    kBonds,
    kCash,
    kRealEstate,
    kInflation,
    kNUM_CLASSES
};

struct AccountWithInterest : Account {
    class_tag(AccountWithInterest, Account);
    explicit AccountWithInterest(Accounts *parent, std::string name, Interest interest)
        : Account(parent, std::move(name)), interest_(interest) {}

    pure USD withdraw(const USD &amount) const override;
    pure Entry project(const DatedEntry &prev, const Day &next) const override;

    pure Interest apy() const { return interest_; }
    void set_apy(const Interest interest) { interest_ = interest; }

    /// Returns the per-year return sequence for this account type, or nullptr to use interest_.
    pure virtual AssetType type() const = 0;

  protected:
    Interest interest_;
};

struct Bonds : AccountWithInterest {
    class_tag(Bonds, AccountWithInterest);
    using AccountWithInterest::AccountWithInterest;
    void tax_gain(const Day &date, const USD &gain) const override;
    void tax_sale(const Day &, const USD &) const override { }
    pure AssetType type() const override { return AssetType::kBonds; }
};

struct Cash : AccountWithInterest {
    class_tag(Cash, AccountWithInterest);
    using AccountWithInterest::AccountWithInterest;
    void tax_gain(const Day &date, const USD &amount) const override;
    void tax_sale(const Day &, const USD &) const override { }
    pure AssetType type() const override { return AssetType::kCash; }
};

struct RealEstate : AccountWithInterest {
    class_tag(RealEstate, AccountWithInterest);
    using AccountWithInterest::AccountWithInterest;
    void tax_gain(const Day &, const USD &) const override { }
    void tax_sale(const Day &date, const USD &gain) const override;
    pure AssetType type() const override { return AssetType::kRealEstate; }
};

struct Retirement : AccountWithInterest {
    class_tag(Retirement, AccountWithInterest);
    enum Kind { kPreTax, kPostTax };

    explicit Retirement(Accounts *parent, std::string name, Interest interest, Kind kind)
        : AccountWithInterest(parent, std::move(name), interest), kind_(kind) {}

    void tax_gain(const Day &, const USD &) const override { }
    void tax_sale(const Day &date, const USD &gain) const override;
    pure AssetType type() const override { return AssetType::kStocks; }

  protected:
    Kind kind_;
};

struct Stocks : AccountWithInterest {
    class_tag(Stocks, AccountWithInterest);

    explicit Stocks(Accounts *parent, std::string name, Interest interest, double dividend_yield = 0.013)
        : AccountWithInterest(parent, std::move(name), interest), dividend_yield_(dividend_yield) {}

    void tax_gain(const Day &date, const USD &gain) const override;
    void tax_sale(const Day &date, const USD &gain) const override;
    pure AssetType type() const override { return AssetType::kStocks; }

  private:
    double dividend_yield_;
};
