#include "Account.h"

#include "invest/Taxes.h"
#include "Accounts.h"

USD Account::balance() const { return parent_->current_entry(this).balance; }

USD Account::deposit(const USD &amount) const {
    parent_->current_entry(this).balance += amount;
    return amount;
}

USD AccountWithInterest::withdraw(const USD &amount) const {
    const Day day = parent_->current_day();
    Entry &entry = parent_->current_entry(this);
    const USD actual = std::min(amount, entry.balance);
    return_if(actual <= 0.00_USD, 0.00_USD);
    const USD gain = std::max(entry.balance - entry.principal, 0_USD);
    const double gain_pct = entry.balance.f64() > 0.0 ? gain.f64() / entry.balance.f64() : 0.0;
    const USD withdrawn_gain      = actual * gain_pct;
    const USD withdrawn_principal = actual * (1.0 - gain_pct);
    tax_sale(day, withdrawn_gain);
    entry.balance -= actual;
    entry.principal -= withdrawn_principal;
    return actual;
}

Entry AccountWithInterest::project(const DatedEntry &prev, const Day &next) const {
    const USD initial = interest_.compounds() ? prev.balance : prev.principal;
    const USD new_bal = interest_.estimate(prev.day, next, initial);
    const USD delta   = new_bal - prev.balance;
    Entry entry;
    entry.principal = prev.principal;
    entry.balance = new_bal;
    tax_gain(next, delta);
    return entry;
}


void Bonds::tax_gain(const Day& date, const USD& gain) const {
    parent_->taxes.income(name_ + " Dividends", date, gain);
}

void Cash::tax_gain(const Day &date, const USD &gain) const {
    parent_->taxes.income(name_ + " Interest", date, gain);
}

void RealEstate::tax_sale(const Day &date, const USD &gain) const {
    static const auto kExclusion = 500000_USD;
    if (const auto remain = gain - kExclusion; remain > 0_USD)
        parent_->taxes.long_term(date, remain);
}

void Retirement::tax_sale(const Day &date, const USD &gain) const {
    return_if(kind_ == kPostTax);
    parent_->taxes.income("Retirement", date, gain);
}

void Stocks::tax_gain(const Day &date, const USD &gain) const { parent_->taxes.dividends(date, gain * 0.1); }
void Stocks::tax_sale(const Day &date, const USD &gain) const { parent_->taxes.long_term(date, gain * 0.9); }
