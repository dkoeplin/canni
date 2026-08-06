#include "Account.h"

#include <ranges>

#include "invest/Taxes.h"

namespace {

using Iter = std::map<Day, Account::Entry>::iterator;
USD project(const Iter &begin, const Iter &end, Iter &iter, const Interest interest) {
    return_if(iter == begin, 0.00_USD); // Skip
    auto &[day_c, c] = *iter;

    const auto &[day_a, a] = *std::prev(iter);
    c.principal = a.principal;
    c.projected = true;

    const I64 elapsed = day_c - day_a; // Time between C and A

    if (auto b_iter = std::next(iter); b_iter != end) {
        // Use a linear projection based on the delta between A and B.
        const auto &[day_b, b] = *b_iter;
        const USD dy = b.balance - a.balance;           // Balance delta between B and A
        const F64 dt = static_cast<F64>(day_b - day_a); // Days between B and A
        const USD slope = USD::round(dy.f64() / dt);    // Change per day
        const USD delta = slope * elapsed;
        c.balance = a.balance + delta;                  // Estimated amount on day C
        return 0.00_USD; // Don't tax these (for now)
    }
    // Use either simple or compounding interest equations.
    const USD &initial = interest.compounds() ? a.balance : a.principal;
    c.balance = interest.estimate(day_a, day_c, initial);
    return c.balance - a.balance; // Return the delta from the original balance
}

} // namespace

Account::Account(const nvl::Tensor<2, std::string> &data, const std::string &name,
                         const Interest &interest, I64 balance, nvl::Maybe<I64> principal)
: Account(name, interest) {
    for (I64 i = 0; i < data.shape()[0]; ++i) {
        const nvl::Pos<2> day_idx (i, 0);
        const nvl::Pos<2> bal_idx (i, balance);
        if (const auto day = Day::parse(data[day_idx])) {
            Entry entry;
            entry.balance = USD::parse(data[bal_idx]);
            if (principal) {
                const nvl::Pos<2> pcp_idx (i, *principal);
                entry.principal = USD::parse(data[pcp_idx]);
            } else {
                entry.principal = entry.balance;
            }
            value_.emplace(*day, entry);
        }
    }
}

std::vector<Day> Account::dates() const {
    return value_ | std::views::keys | std::ranges::to<std::vector>();
}

void Account::ensure(Taxes &taxes, const Day &date) const {
    auto [iter, inserted] = value_.try_emplace(date, 0.00_USD);
    return_if(!inserted);
    const USD delta = project(value_.begin(), value_.end(), iter, interest_);
    this->tax_gain(taxes, date, delta);
}

void Account::deposit(const Day &date, const USD &amount) {
    const auto iter = value_.upper_bound(date);
    return_if(value_.empty() || iter == value_.begin());
    for (auto i = std::prev(iter); i != value_.end(); ++i) {
        i->second.balance += amount;
        i->second.principal += amount;
    }
}

USD Account::withdraw(Taxes &taxes, const Day &date, const USD &amount) {
    const auto iter = value_.upper_bound(date);
    return_if(value_.empty() || iter == value_.begin(), 0.00_USD);
    // At or before this date
    const auto start = std::prev(iter);

    USD withdrawn = amount;
    for (auto i = start; i != value_.end(); ++i) {
        const USD balance = i->second.balance;
        const USD withdraw = std::max(0_USD, std::min(amount, balance));
        withdrawn = std::min(withdrawn, withdraw);
    }
    return_if(withdrawn <= 0.00_USD, 0.00_USD);

    // Register taxes on the actual amount withdrawn prior to decreasing the balance.
    // Assumes that the total gain is representative of the gain on the portion sold.
    // e.g. if the principal was $75, the current balance is $100, assume 25% of the withdrawal is taxable gain.
    // Then, for a withdrawal of e.g. $10, $2.50 is taxed gain, $7.50 is withdrawn from principal.
    const auto entry = get(date);
    const auto gain = std::max(entry.balance - entry.principal, 0_USD);
    const double gain_percent = gain.f64() / entry.balance.f64();
    const USD withdrawn_gain = withdrawn * gain_percent;
    const USD withdrawn_principal = withdrawn * (1.0 - gain_percent);
    this->tax_sale(taxes, date, withdrawn_gain, withdrawn);

    for (auto i = start; i != value_.end(); ++i) {
        i->second.balance -= withdrawn;
        i->second.principal -= withdrawn_principal;
    }
    return withdrawn;
}

Account::Entry Account::get(const Day &date) const {
    const auto iter = value_.upper_bound(date);
    return_if(value_.empty() || iter == value_.begin(), {});
    return std::prev(iter)->second;
}
