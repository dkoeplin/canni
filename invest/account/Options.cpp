#include "Options.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
#include <utility>

#include "Accounts.h"
#include "invest/Parsing.h"

Options::Options(Accounts *parent, Company company, const std::string &name, const Type type, USD strike, const std::string &csv_path)
    : Account(parent, name), company_(std::move(company)), type_(type), strike_(std::move(strike)) {
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

Entry Options::project(const DatedEntry &, const Day &next) const {
    const auto [iso, nso] = avail(next);
    const USD fmv = company_.fmv_at(next);
    return Entry {.principal = 0.00_USD, .balance = fmv * (iso + nso) };
}

Entry Options::estimate(const Day &day) const {
    const auto [iso, nso] = avail(day);
    const USD fmv = company_.fmv_at(day);
    return Entry {.principal = 0.00_USD, .balance = fmv * (iso + nso) };
}

Options::Sale Options::sell(const Day day, const USD &price, Count count, bool cashless) {
    const auto available = avail(day);
    count.iso = std::min(available.iso, count.iso);
    count.nso = std::min(available.nso, count.nso);

    // Update the number sold on and after this day for future checks of remaining options
    const auto prior = sold(day);
    auto [iter, inserted] = sold_.try_emplace(day, prior);
    for (; iter != sold_.end(); ++iter) {
        iter->second += count;
    }

    const USD fmv = company_.fmv_at(day);

    // ISO and NSO have essentially the same tax implications for same-day sales, just add them here.
    const I64 n = count.iso + count.nso;
    const USD spread = (fmv - strike_) * n;
    const USD gain = (price - fmv) * n;

    Taxes &taxes = parent_->taxes;
    // Spread (FMV - Strike) is taxed as standard income.
    taxes.income(name_ + " Exercise on " + day.to_string(), day, spread);
    // Gain (Sale - FMV) is taxed as short term capital gains.
    taxes.short_term(day, gain);

    // Estimate withholding for supplemental income.
    const auto withholding = std::min(1e6_USD, spread) * 0.22 + std::max(0_USD, spread - 1e6_USD) * 0.37;
    const auto medicare = spread * 0.0235;
    taxes.withholding(day, withholding);
    taxes.medicare(day, medicare);
    const auto costs = strike_ * n + withholding + medicare;

    Sale sale;
    sale.costs = cashless ? 0.00_USD : costs;
    sale.proceeds = cashless ? price * n - costs : price * n;
    return sale;
}

Options &Options::with_sale(Day day, const USD &price, Count count, bool cashless) {
    (void)sell(day, price, count, cashless);
    return *this;
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
