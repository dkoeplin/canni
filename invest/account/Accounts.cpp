#include "Accounts.h"

#include <cmath>
#include <ranges>
#include <set>
#include <unordered_map>

#include "Options.h"
#include "invest/Inflation.h"
#include "nvl/data/Tensor.h"

#include "invest/Parsing.h"

const Accounts::ColumnType Accounts::Ignore = ColumnType(nullptr, ColumnType::kIgnore);

namespace {

void sample_returns(std::vector<double> &asset, std::vector<double> &cumulative,
                    I64 years, double mean, double sigma, std::mt19937 &rng) {
    // Log-normal: draw log(1+r) ~ Normal(log_mu, sigma) so that E[1+r] = 1+mean.
    const double log_mu = std::log(1.0 + mean) - 0.5 * sigma * sigma;
    std::normal_distribution<double> dist(log_mu, sigma);
    asset = std::vector<double>(years);
    cumulative = std::vector<double>(years);
    for (I64 i = 0; i < years; ++i) {
        asset[i] = std::exp(dist(rng)) - 1.0;
        cumulative[i] = i == 0 ? 1.0 : cumulative[i - 1] * (1.0 + asset[i - 1]);
    }
}

} // namespace

MarketReturns::MarketReturns(const I64 base_year, const I64 years, const std::vector<AssetClass> &classes, std::mt19937 &rng)
    : base_year_(base_year) {
    classes_.resize(static_cast<U64>(AssetType::kNUM_CLASSES));
    cumulative_.resize(static_cast<U64>(AssetType::kNUM_CLASSES));
    for (const auto &[type, mean, sigma] : classes) {
        auto &asset = classes_[static_cast<U64>(type)];
        auto &cumulative = cumulative_[static_cast<U64>(type)];
        sample_returns(asset, cumulative, years, mean, sigma, rng);
    }
}

pure double MarketReturns::at(const AssetType type, I64 year) const {
    const auto &sequence = classes_.at(static_cast<U64>(type));
    const I64 idx = year - base_year_;
    return (idx >= 0 && idx < static_cast<I64>(sequence.size())) ? sequence[idx] : 0.0;
}

pure double MarketReturns::cumulative(const AssetType type, I64 year) const {
    const auto &sequence = cumulative_.at(static_cast<U64>(type));
    const I64 idx = year - base_year_;
    return (idx >= 0 && idx < static_cast<I64>(sequence.size())) ? sequence[idx] : 0.0;
}

void Accounts::import_csv(const std::string &filename, const std::vector<ColumnType> &columns) {
    const nvl::Tensor<2,std::string> data = parse_data(filename);
    for (I64 i = 0; i < data.shape()[0]; ++i) {
        const nvl::Pos<2> day_idx (i, 0);
        if (const auto day = Day::parse(data[day_idx])) {
            dates_.push_back(*day);
            for (const auto &account : accounts_) {
                rows_[account->index()].push_back(account->estimate(*day));
            }
            for (I64 j = 0; j < static_cast<I64>(columns.size()); ++j) {
                const auto &col = columns[j];
                const nvl::Pos<2> index (i, j + 1);
                if (col.type == ColumnType::kIgnore) {
                    // Skip
                } else if (col.type == ColumnType::kPrincipal) {
                    rows_[col.account->index()].back().principal = USD::parse(data[index]);
                } else if (col.type == ColumnType::kBalance) {
                    rows_[col.account->index()].back().balance = USD::parse(data[index]);
                }
            }
        }
    }
}

XYSeries Accounts::totals(const std::string &name, std::optional<Inflation> inflation) const {
    XYSeries series;
    series.name = name;
    for (U64 i = 0; i < dates_.size(); ++i) {
        const Day &day = dates_.at(i);
        USD total;
        for (const std::string_view &group : groups_) {
            if (group != Options::_classtag.name && group != RealEstate::_classtag.name) {
                for (const auto &account : accounts_by_group_.at(group)) {
                    auto balance = rows_.at(account->index()).at(i).balance;
                    total += inflation ? inflation->inverse(day, balance) : balance;
                }
            }
        }
        series.points.emplace_back(dates_.at(i).to_string("%Y-%m-%d"), total.f64());
    }
    return series;
}

std::vector<XYSeries> Accounts::grouped_totals(std::optional<Inflation> inflation) const {
    std::vector<XYSeries> groups;
    for (const auto &group : groups_) {
        XYSeries series;
        series.name = group;
        groups.push_back(series);
    }
    for (U64 i = 0; i < dates_.size(); ++i) {
        const auto &day = dates_.at(i);
        const auto day_str = day.to_string("%Y-%m-%d");
        for (U64 j = 0; j < groups_.size(); ++j) {
            const auto &group = groups_.at(j);
            USD group_total;
            for (const auto &account : accounts_by_group_.at(group)) {
                const auto balance = rows_.at(account->index()).at(i).balance;
                group_total += inflation ? inflation->inverse(day, balance) : balance;
            }
            groups[j].points.emplace_back(day_str, group_total.f64());
        }
    }
    return groups;
}

USD Accounts::total() const {
    USD total;
    for (const auto &account : accounts_) {
        total += account->balance();
    }
    return total;
}

Entry &Accounts::current_entry(const Account *account) {
    ASSERT(account->index() < rows_.size(), "No entry for account " << account->name());
    ASSERT(!rows_.at(account->index()).empty(), "No entries defined for account " << account->name());
    return rows_.at(account->index()).back();
}

void Accounts::seed(const Day &day) {
    dates_.push_back(day);
    for (const auto &account : accounts_) {
        rows_[account->index()].push_back(account->estimate(day));
    }
}

Accounts &Accounts::project(const Day::Distance step) {
    ASSERT(!dates_.empty(), "Accounts::project requires account history.");
    const Day prev_day = dates_.back();
    const Day next_day = prev_day + step;

    // Advance each account
    for (const auto &account : accounts_) {
        auto &entries = rows_.at(account->index());
        const DatedEntry prev_entry (prev_day, entries.back());
        entries.push_back(account->project(prev_entry, next_day));
    }
    dates_.push_back(next_day);

    // Fire events after all account balances are updated
    for (const auto &event : events_) {
        event->evaluate(next_day);
    }
    return *this;
}

Accounts &Accounts::project_until(const Day &last, const Day::Distance &step) {
    ASSERT(!dates_.empty(), "Accounts::project_until requires account history.");
    while (dates_.back() < last) {
        project(step);
    }
    return *this;
}

Account &Accounts::add(const std::shared_ptr<Account> &account) {
    static constexpr std::vector<Account *> kEmptyGroup = {};
    static constexpr std::vector<Entry> kEmptyEntries = {};

    const auto &ref = accounts_.emplace_back(account);
    ref->set_index(accounts_.size() - 1);
    const auto [group, inserted] = accounts_by_group_.try_emplace(account->category(), kEmptyGroup);
    if (inserted) {
        groups_.push_back(account->category());
    }
    group->second.push_back(ref.get());

    // Ensure the new account has a full history to date
    auto &row = rows_.emplace_back(kEmptyEntries);
    while (row.size() < dates_.size()) {
        const auto day = dates_.at(row.size());
        row.push_back(account->estimate(day));
    }

    return *ref;
}

Event &Accounts::add(const std::shared_ptr<Event> &event) {
    return *events_.emplace_back(event);
}
