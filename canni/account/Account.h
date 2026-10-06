#pragma once

#include <algorithm>
#include <memory>
#include <ranges>
#include <utility>

#include "canni/Macros.h"
#include "canni/data/Day.h"
#include "canni/data/Interest.h"
#include "canni/data/USD.h"

namespace canni {

struct Portfolio;

struct AccountSnapshot {
    virtual ~AccountSnapshot() = default;
};

struct Entry {
    USD principal;
    USD balance;
};

struct DatedEntry : Entry {
    explicit DatedEntry(const Day &day, const Entry &entry) : Entry(entry), day(day) {}
    Day day;
};

abstract struct Account {
    enum Kind : U64 {
        kBonds,
        kCash,
        kMortgage,
        kRealEstate,
        kRetirement,
        kStocks,
        kOptions,
        kInflation,
        kNUM_ACCOUNT_TYPES
    };

    explicit Account(Portfolio *parent, std::string name) : parent_(parent), name_(std::move(name)) {}
    virtual ~Account() = default;

    pure virtual std::shared_ptr<AccountSnapshot> save() const { return nullptr; }
    virtual void restore(const AccountSnapshot &) {}

    pure virtual Kind kind() const = 0;

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

    /// Taxes incurred on the gain portion during a withdrawal.
    virtual void tax_sale(const Day &day, const USD &gain) const = 0;

    /// Taxes incurred on the principal portion during a withdrawal (pre-tax accounts only).
    virtual void tax_principal(const Day &, const USD &) const {}

    pure const std::string &name() const { return name_; }

    void set_index(const U64 index) { index_ = index; }
    pure U64 index() const { return index_; }

  protected:
    Portfolio *parent_;
    std::string name_;
    U64 index_ = UINT64_MAX;
};

template <Account::Kind K>
struct Investment : Account {
    explicit Investment(Portfolio *parent, std::string name, Interest interest)
        : Account(parent, std::move(name)), interest_(interest) {}

    static constexpr Kind kKind = K;
    pure Kind kind() const override { return K; }
    pure USD withdraw(const USD &amount) const override;
    pure Entry project(const DatedEntry &prev, const Day &next) const override;

    pure Interest apy() const { return interest_; }
    void set_apy(const Interest interest) { interest_ = interest; }

  protected:
    Interest interest_;
};

struct Bonds : Investment<Account::kBonds> {
    using Investment::Investment;
    void tax_gain(const Day &date, const USD &gain) const override;
    void tax_sale(const Day &, const USD &) const override { }
};

struct Cash : Investment<Account::kCash> {
    using Investment::Investment;
    void tax_gain(const Day &date, const USD &amount) const override;
    void tax_sale(const Day &, const USD &) const override { }
};

struct RealEstate : Investment<Account::kRealEstate> {
    using Investment::Investment;
    void tax_gain(const Day &, const USD &) const override { }
    void tax_sale(const Day &date, const USD &gain) const override;
};

struct Retirement : Investment<Account::kRetirement> {
    enum Type { kPreTax, kPostTax };

    explicit Retirement(Portfolio *parent, std::string name, Interest interest, Type type)
        : Investment(parent, std::move(name), interest), type_(type) {}

    pure USD withdraw(const USD &amount) const override;
    void tax_gain(const Day &, const USD &) const override { }
    void tax_sale(const Day &date, const USD &gain) const override;
    void tax_principal(const Day &date, const USD &principal) const override;
    pure Type type() const { return type_; }

  protected:
    Type type_;
};

struct Stocks : Investment<Account::kStocks> {
    explicit Stocks(Portfolio *parent, std::string name, Interest interest, double dividend_yield = 0.013)
        : Investment(parent, std::move(name), interest), dividend_yield_(dividend_yield) {}

    void tax_gain(const Day &date, const USD &gain) const override;
    void tax_sale(const Day &date, const USD &gain) const override;

  private:
    double dividend_yield_;
};

/**
 * @struct Liability
 * @brief Base class for liabilities (loans, mortgages, etc.).
 * Balance is stored as a negative value so Accounts::total() naturally subtracts it.
 * deposit() reduces the outstanding debt; withdraw() is a no-op.
 */
template <Account::Kind K>
abstract struct Liability : Account {
    using Account::Account;
    static constexpr Kind kKind = K;
    pure Kind kind() const override { return K; }
    pure USD withdraw(const USD &) const override { return 0_USD; }
    void tax_gain(const Day &, const USD &) const override {}
    void tax_sale(const Day &, const USD &) const override {}
    pure Entry project(const DatedEntry &prev, const Day &) const override { return {prev.principal, prev.balance}; }
};

struct Mortgage : Liability<Account::kMortgage> {
    struct Params {
        double down_pct = 0.20;     // Down payment as a fraction of purchase price
        double annual_rate = 0.07;  // Annual interest rate
        I64 duration_years = 30;    // Loan term in yearsZ
        USD extra_monthly = 0_USD;  // Additional monthly principal payment above the required amount
    };

    explicit Mortgage(Portfolio *parent, std::string name, Params params)
        : Liability(parent, std::move(name)), params_(std::move(params)) {}

    /// Called at purchase time with the actual loan amount to initialize the balance
    /// and compute the fixed monthly payment via standard amortization.
    void originate(const USD &loan);

    pure USD owed() const { return -balance(); }
    pure USD monthly_payment() const { return monthly_payment_; }
    pure const Params &params() const { return params_; }

  private:
    Params params_;
    USD monthly_payment_ = 0_USD;
};

std::string_view to_string(Account::Kind kind);

} // namespace canni
