#include "Account.h"

#include <ranges>

#include "invest/Taxes.h"

Account::Account(const nvl::Tensor<2, std::string> &data, const std::string &name,
                 const Interest &interest, I64 balance_col, nvl::Maybe<I64> principal_col)
: Account(name, interest) {
    for (I64 i = 0; i < data.shape()[0]; ++i) {
        const nvl::Pos<2> day_idx (i, 0);
        const nvl::Pos<2> bal_idx (i, balance_col);
        if (const auto day = Day::parse(data[day_idx])) {
            Entry entry;
            entry.balance = USD::parse(data[bal_idx]);
            if (principal_col) {
                const nvl::Pos<2> pcp_idx (i, *principal_col);
                entry.principal = USD::parse(data[pcp_idx]);
            } else {
                entry.principal = entry.balance;
            }
            history_.emplace(*day, entry);
        }
    }
    // Initialize current state from the last historical entry.
    if (!history_.empty()) {
        const auto &last = history_.rbegin()->second;
        balance_   = last.balance;
        principal_ = last.principal;
    }
}

std::vector<Day> Account::dates() const {
    return history_ | std::views::keys | std::ranges::to<std::vector>();
}

Account::Entry Account::get(const Day &date) const {
    const auto iter = history_.upper_bound(date);
    return_if(history_.empty() || iter == history_.begin(), {});
    return std::prev(iter)->second;
}

void Account::advance(Taxes &taxes, const Day &from, const Day &to) {
    const USD initial = interest_.compounds() ? balance_ : principal_;
    const USD new_bal = interest_.estimate(from, to, initial);
    const USD delta   = new_bal - balance_;
    balance_ = new_bal;
    tax_gain(taxes, to, delta);
}

USD Account::withdraw(Taxes &taxes, const Day &date, const USD &amount) {
    const USD actual = std::min(amount, balance_);
    return_if(actual <= 0.00_USD, 0.00_USD);
    const USD gain = std::max(balance_ - principal_, 0_USD);
    const double gain_pct = balance_.f64() > 0.0 ? gain.f64() / balance_.f64() : 0.0;
    const USD withdrawn_gain      = actual * gain_pct;
    const USD withdrawn_principal = actual * (1.0 - gain_pct);
    tax_sale(taxes, date, withdrawn_gain, actual);
    balance_   -= actual;
    principal_ -= withdrawn_principal;
    return actual;
}
