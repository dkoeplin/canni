#include "Options.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
#include <utility>

#include "invest/Parsing.h"

Options::Options(Company company, const std::string &name, const Type type, USD strike, const std::string &csv_path)
    : Account(name, 0.00_pct), company_(std::move(company)), type_(type), strike_(std::move(strike)) {
    company_.add_options(this);
    return_if(csv_path.empty());

    const nvl::Tensor<2, std::string> data = parse_data(csv_path, '\t');
    const I64 expected_cols = type == kMix ? 7 : 5;
    ASSERT(data.shape()[1] == expected_cols,
        csv_path << ": Found malformed options file while loading options \"" << name << "\":\n"
        "Expected " << expected_cols << " columns, but got " << data.shape()[1] << ".");

    std::map<Day, Count> schedule; /// Date -> new shares vesting on that date
    for (I64 i = 0; i < data.shape()[0]; ++i) {
        if (const auto day = Day::parse(data[{i, 1}], "%b %d %Y")) {
            if (type == kISO) {
                schedule[*day].iso = std::stoll(data[{i, 2}]);
            } else if (type == kNSO) {
                schedule[*day].nso = std::stoll(data[{i, 2}]);
            } else {
                schedule[*day].iso = std::stoll(data[{i, 3}]);
                schedule[*day].nso = std::stoll(data[{i, 4}]);
            }
        } else {
            std::cout << "Failed to parse line: " << std::endl << "|";
            for (I64 j = 0; j < data.shape()[1]; ++j) {
                std::cout << data[{i, j}] << "|";
            }
            std::cout << "|" << std::endl;
            std::abort();
        }
    }
    Count total;
    for (const auto &[day, count] : schedule) {
        if (!expires_) {
            set_exp(day + 10_years);
        }
        total += count;
        vested_[day] = total;
    }
}

void Options::print() const {
    std::cout << name_ << std::endl;
    for (const auto &day : dates()) {
        std::cout << "  [" << day << "] vested: " << vested(day) << ", sold: " << sold(day) << std::endl;
    }
}

pure std::vector<Day> Options::dates() const {
    std::set<Day> dates;
    const auto sold_dates = sold_ | std::views::keys;
    const auto vest_dates = vested_ | std::views::keys;
    dates.insert(sold_dates.begin(), sold_dates.end());
    dates.insert(vest_dates.begin(), vest_dates.end());
    return {dates.begin(), dates.end()};
}

Options &Options::vesting(const Day &day, Count count) {
    const auto prev = vested(day);                         // Total vested on this day, prior to the new options.
    const auto [iter, _] = vested_.try_emplace(day, prev); // Add explicit entry for this day if it didn't exist
    for (auto i = iter; i != vested_.end(); ++i) {         // Increment every entry at or after this day
        i->second += count;
    }
    return *this;
}

Options::Sale Options::sell(Taxes &taxes, const Day &day, const USD &price, const Count count, bool cashless) {
    const auto prev = sold(day);                         // Total sold on this day, prior to the newly sold shares.
    const auto [iter, _] = sold_.try_emplace(day, prev); // Add explicit entry for this day if it didn't exist
    for (auto i = iter; i != sold_.end(); ++i) {         // Increment every entry at or after this day
        i->second += count;
    }
    // Assumes short term (same-day) sale, so spread as standard income and sale gains as short-term capital gains
    const USD fmv = company_.fmv_at(day);
    const I64 n = count.iso + count.nso; // ISO and NSO have essentially the same tax implications for same-day sales
    const USD spread = (fmv - strike_) * n;
    const USD gain = (price - fmv) * n;
    taxes.income(name_ + " Exercise on " + day.to_string(), day, spread); // Spread (FMV - Strike) is taxed as standard income.
    taxes.short_term(day, gain); // Gain (Sale - FMV) is taxed as short term capital gains.

    // Estimate withholding for supplemental income.
    const auto withholding = std::min(1e6_USD, spread) * 0.22 + std::max(0_USD, spread - 1e6_USD) * 0.37;
    const auto medicare = spread * 0.0235;
    taxes.withholding(day, withholding);
    taxes.medicare(day, medicare);
    const auto costs = strike_ * n + withholding + medicare;

    Sale sale;
    sale.exercise = cashless ? 0.00_USD : costs;
    sale.proceeds = cashless ? price * n - costs : price * n;
    return sale;
}

Options::Count Options::vested(const Day &day) const {
    return_if(expires_ && day >= *expires_, {}); // Expiration
    const auto it = vested_.upper_bound(day); // Strictly after this day
    return_if(it == vested_.begin() || vested_.empty(), {});
    const auto iter = std::prev(it);
    return iter->second;
}

Options::Count Options::sold(const Day &day) const {
    return_if(expires_ && day >= *expires_, {}); // Expiration
    const auto it = sold_.upper_bound(day); // Strictly after this day
    return_if(it == sold_.begin() || sold_.empty(), {});
    const auto iter = std::prev(it);
    return iter->second;
}

Account::Entry Options::get(const Day &day) const {
    const auto [iso, nso] = vested(day) - sold(day);
    return_if(iso == 0 && nso == 0, {});
    const USD price_per_share = company_.market(day);
    const USD gain_per_share = price_per_share - strike_;
    return_if(gain_per_share <= 0.00_USD, {});
    return {.principal = 0.00_USD, .balance = gain_per_share * (iso + nso)};
}
