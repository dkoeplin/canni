#include "Accounts.h"

#include <ranges>
#include <set>
#include <unordered_map>

#include "nvl/data/Tensor.h"

#include "invest/Parsing.h"

const Accounts::ColumnType Accounts::Ignore = ColumnType(nullptr, ColumnType::kIgnore);

void Accounts::parse_history(const std::string &filename, const std::vector<ColumnType> &columns) {
    const nvl::Tensor<2,std::string> data = parse_data(filename);
    for (I64 i = 0; i < data.shape()[0]; ++i) {
        const nvl::Pos<2> day_idx (i, 0);
        if (const auto day = Day::parse(data[day_idx])) {
            dates_.push_back(*day);
            for (const auto &account : accounts_) {
                rows_[account.get()].push_back(account->estimate(*day));
            }
            for (I64 j = 0; j < static_cast<I64>(columns.size()); ++j) {
                const auto &col = columns[j];
                const nvl::Pos<2> index (i, j + 1);
                if (col.type == ColumnType::kIgnore) {
                    // Skip
                } else if (col.type == ColumnType::kPrincipal) {
                    rows_[col.account].back().principal = USD::parse(data[index]);
                } else if (col.type == ColumnType::kBalance) {
                    rows_[col.account].back().balance = USD::parse(data[index]);
                }
            }
        }
    }
}

USD Accounts::GroupTotals::cumulative(std::string_view group) const {
    USD cumulative;
    for (auto iter = groups.begin(); iter != groups.end(); ++iter) {
        cumulative += iter->second;
        return_if(iter->first == group, cumulative);
    }
    return cumulative;
}

std::vector<Accounts::GroupTotals> Accounts::totals() const {
    std::vector<GroupTotals> totals;
    for (U64 i = 0; i < dates_.size(); ++i) {
        GroupTotals &entry = totals.emplace_back();
        entry.day = dates_[i];
        for (const std::string_view &group : groups_) {
            USD group_total;
            for (const auto &account : accounts_by_group_.at(group)) {
                group_total += rows_.at(account).at(i).balance;
            }
            entry.groups.emplace_back(group, group_total);
            entry.total += group_total;
        }
    }
    return totals;
}

USD Accounts::total() const {
    USD total;
    for (const auto &account : accounts_) {
        total += account->balance();
    }
    return total;
}

Entry &Accounts::current_entry(const Account *account) {
    const auto iter = rows_.find(account);
    ASSERT(iter != rows_.end(), "No entry for account " << account->name());
    ASSERT(!iter->second.empty(), "No entries defined for account " << account->name());
    return iter->second.back();
}

void Accounts::project(const Day::Distance step) {
    const Day prev_day = dates_.back();
    const Day next_day = prev_day + step;

    // Advance each account
    for (const auto &account : accounts_) {
        auto &entries = rows_.at(account.get());
        const DatedEntry prev_entry (prev_day, entries.back());
        entries.push_back(account->project(prev_entry, next_day));
    }
    dates_.push_back(next_day);

    // Fire events after all account balances are updated
    for (const auto &event : events_) {
        event->evaluate(next_day);
    }
}

void Accounts::project_until(const Day &last, const Day::Distance &step) {
    while (dates_.back() < last) {
        project(step);
    }
}

Account &Accounts::add(const std::shared_ptr<Account> &account) {
    static constexpr std::vector<Account *> kEmptyGroup = {};
    static constexpr std::vector<Entry> kEmptyEntries = {};

    const auto &ref = accounts_.emplace_back(account);
    const auto [group, inserted] = accounts_by_group_.try_emplace(account->category(), kEmptyGroup);
    if (inserted) {
        groups_.push_back(account->category());
    }
    group->second.push_back(ref.get());

    // Ensure the new account has a full history to date
    const auto [entries, _] = rows_.try_emplace(ref.get(), kEmptyEntries);
    while (entries->second.size() < dates_.size()) {
        const auto day = dates_.at(entries->second.size());
        entries->second.push_back(account->estimate(day));
    }

    return *ref;
}

Event &Accounts::add(const std::shared_ptr<Event> &event) {
    return *events_.emplace_back(event);
}
