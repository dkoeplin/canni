#include "Account.h"

#include <cmath>

#include "canni/account/Taxes.h"
#include "canni/account/Portfolio.h"

namespace canni {

USD Account::balance() const { return parent_->current_entry(this).balance; }

USD Account::deposit(const USD &amount) const {
    parent_->current_entry(this).balance += amount;
    return amount;
}

template <Account::Kind K>
USD Investment<K>::withdraw(const USD &amount) const {
    const Day day = parent_->current_day();
    Entry &entry = parent_->current_entry(this);
    const USD actual = std::min(amount, entry.balance);
    return_if(actual <= 0.00_USD, 0.00_USD);
    const USD gain = std::max(entry.balance - entry.principal, 0_USD);
    const double gain_pct = entry.balance.f64() > 0.0 ? gain.f64() / entry.balance.f64() : 0.0;
    const USD withdrawn_gain      = actual * gain_pct;
    const USD withdrawn_principal = actual * (1.0 - gain_pct);
    tax_sale(day, withdrawn_gain);
    tax_principal(day, withdrawn_principal);
    entry.balance -= actual;
    entry.principal -= withdrawn_principal;
    return actual;
}

template <Account::Kind K>
Entry Investment<K>::project(const DatedEntry &prev, const Day &next) const {
    Interest effective = interest_;
    if (const auto *returns = parent_->market_returns()) {
        effective = Interest(interest_.type(), returns->at(kind(), next.year()));
    }
    const USD initial = effective.compounds() ? prev.balance : prev.principal;
    const USD new_bal = effective.estimate(prev.day, next, initial);
    const USD delta   = new_bal - prev.balance;
    Entry entry;
    entry.principal = prev.principal;
    entry.balance = new_bal;
    tax_gain(next, delta);
    return entry;
}

void Bonds::tax_gain(const Day& date, const USD& gain) const {
    parent_->taxes.income(date, gain);
}

void Cash::tax_gain(const Day &date, const USD &gain) const {
    parent_->taxes.income(date, gain);
}

void RealEstate::tax_sale(const Day &date, const USD &gain) const {
    static constexpr auto kExclusion = 500000_USD;
    if (const auto remain = gain - kExclusion; remain > 0_USD)
        parent_->taxes.long_term(date, remain);
}

USD Retirement::withdraw(const USD &amount) const {
    const auto today = parent_->current_day();
    const USD actual = Investment::withdraw(amount);
    parent_->taxes.withdraw_retirement(today, amount);
    return actual;
}

void Retirement::tax_sale(const Day &date, const USD &gain) const {
    return_if(type_ == kPostTax);
    parent_->taxes.income(date, gain);
}
void Retirement::tax_principal(const Day &date, const USD &principal) const {
    return_if(type_ == kPostTax);
    parent_->taxes.income(date, principal);
}

void Stocks::tax_gain(const Day &date, const USD &) const {
    // Dividends are modeled as a fixed yield on the current balance, independent of price direction.
    const USD balance = parent_->current_entry(this).balance;
    parent_->taxes.dividends(date, balance * (dividend_yield_ / 12.0));
}
void Stocks::tax_sale(const Day &date, const USD &gain) const {
    parent_->taxes.long_term(date, gain * (1.0 - dividend_yield_));
}

void Mortgage::originate(const USD &loan) {
    const double r = params_.annual_rate / 12.0;
    const I64 n = params_.duration_years * 12;
    const double factor = std::pow(1.0 + r, static_cast<double>(n));
    monthly_payment_ = USD::round(loan.f64() * r * factor / (factor - 1.0));
    parent_->current_entry(this).balance = -loan;
    parent_->current_entry(this).principal = -loan;
}

std::string_view to_string(const Account::Kind kind) {
    static std::vector<std::string_view> names {
        "Bonds", "Cash", "Mortgage", "Real Estate", "Retirement", "Stocks", "Options", "Inflation"
    };
    return kind < names.size() ? names[kind] : "?";
}


} // namespace canni
