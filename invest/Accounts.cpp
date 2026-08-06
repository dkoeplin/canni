#include "Accounts.h"

#include <algorithm>
#include <numeric>
#include <ranges>
#include <set>
#include <unordered_map>

#include "Taxes.h"

// Returns the row in [rows] whose date is at or before [date], or the first row if all are after.
static const std::vector<USD> &find_row(const std::vector<Accounts::Row> &rows, const Day &date) {
    ASSERT(!rows.empty(), "find_row called on empty rows");
    // Binary search: find last row with date <= query date
    auto it = std::upper_bound(rows.begin(), rows.end(), date,
        [](const Day &d, const Accounts::Row &r) { return d < r.date; });
    if (it == rows.begin()) return rows.front().group_totals;
    return std::prev(it)->group_totals;
}

const std::vector<USD> &Accounts::find_row(const Day &date) const {
    return ::find_row(rows_, date);
}

std::vector<USD> Accounts::operator[](const Day &date) const { return totals(date); }

USD Accounts::cumulative(const std::string_view &group, const Day &date) const {
    const auto group_iter = std::ranges::find(groups_, group);
    return_if(group_iter == groups_.end(), 0.00_USD);
    const auto distance = std::distance(groups_.begin(), group_iter);
    const auto &totals = find_row(date);
    return std::accumulate(totals.begin(), std::next(totals.begin(), distance + 1), 0.00_USD);
}

USD Accounts::total(const Day &date) const {
    const auto &row = find_row(date);
    return std::accumulate(row.begin(), row.end(), 0.00_USD);
}

const std::vector<USD> &Accounts::totals(const Day &date) const {
    return find_row(date);
}

std::vector<std::pair<std::string_view, USD>> Accounts::tagged_totals(const Day &date) const {
    const auto &row = find_row(date);
    std::vector<std::pair<std::string_view, USD>> tagged;
    tagged.reserve(row.size());
    for (U64 i = 0; i < row.size(); ++i) {
        tagged.emplace_back(groups_[i], row[i]);
    }
    return tagged;
}

void Accounts::project(const Day start, const Day ending) {
    rows_.clear();

    // Precompute the last historical date for each account.
    std::unordered_map<const Account *, Day> last_hist;
    for (const auto &[group, accts] : accounts_) {
        for (const auto &acct : accts) {
            const auto d = acct->dates();
            if (!d.empty()) last_hist[acct.get()] = d.back();
        }
    }

    Day prev = start;
    Day curr = start;
    while (curr <= ending) {
        // Advance or seed each account.
        for (const std::string_view &group : groups_) {
            for (const auto &acct : accounts_.at(group)) {
                const auto lit = last_hist.find(acct.get());
                if (lit != last_hist.end() && curr <= lit->second) {
                    // Use historical CSV data.
                    const auto e = acct->get(curr);
                    acct->balance_   = e.balance;
                    acct->principal_ = e.principal;
                } else {
                    acct->advance(taxes, prev, curr);
                }
            }
        }

        // Fire events.
        for (const auto &event : events_) {
            event->evaluate(curr);
        }

        // Build and store row.
        Row row;
        row.date = curr;
        for (const std::string_view &group : groups_) {
            USD group_total;
            for (const auto &acct : accounts_.at(group)) {
                group_total += acct->balance_;
            }
            row.group_totals.push_back(group_total);
        }
        rows_.push_back(std::move(row));

        prev = curr;
        curr = curr.next_month();
    }
}

Account &Accounts::add(const std::shared_ptr<Account> &account) {
    static constexpr std::vector<std::shared_ptr<Account>> kEmpty = {};
    const auto [iter, inserted] = accounts_.try_emplace(account->category(), kEmpty);
    if (inserted) {
        groups_.push_back(account->category());
    }
    return *iter->second.emplace_back(account);
}

Event &Accounts::add(const std::shared_ptr<Event> &event) {
    return *events_.emplace_back(event);
}
