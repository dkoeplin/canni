#include "Accounts.h"

#include <numeric>
#include <ranges>
#include <set>

#include "Taxes.h"

const std::vector<USD> &Accounts::get_or_project(const Day &date) const {
    auto [iter, inserted] = dates_.try_emplace(date, std::vector<USD>{});
    return_if(!inserted, iter->second);

    // Evaluate all accounts first to ensure that they're populated
    for (const std::string_view &group : groups_) {
        for (const auto &account : accounts_.at(group)) {
            account->ensure(taxes, date);
        }
    }

    // Evaluate the effects of events on this date.
    for (const auto &event : events_) {
        event->evaluate(date);
    }

    // Collect the totals
    for (const std::string_view &group : groups_) {
        USD &total = iter->second.emplace_back(0.00_USD);
        for (const auto &account : accounts_.at(group)) {
            total += (*account)[date];
        }
    }
    return iter->second;
}

std::vector<USD> Accounts::operator[](const Day &date) const { return totals(date); }

USD Accounts::cumulative(const std::string_view &group, const Day &date) const {
    const auto group_iter = std::ranges::find(groups_, group);
    return_if(group_iter == groups_.end(), 0.00_USD);
    const auto distance = std::distance(groups_.begin(), group_iter);
    const auto &balances = get_or_project(date);
    return std::accumulate(balances.begin(), std::next(balances.begin(), distance + 1), 0.00_USD);
}

USD Accounts::total(const Day &date) const {
    const auto totals = this->totals(date);
    return std::accumulate(totals.begin(), totals.end(), 0.00_USD);
}

const std::vector<USD> &Accounts::totals(const Day &date) const {
    return get_or_project(date);
}

std::vector<std::pair<std::string_view, USD>> Accounts::tagged_totals(const Day &date) const {
    const auto totals = this->totals(date);
    std::vector<std::pair<std::string_view, USD>> tagged;
    tagged.reserve(totals.size());
    for (U64 i = 0; i < totals.size(); ++i) {
        tagged.emplace_back(groups_[i], totals[i]);
    }
    return tagged;
}

void Accounts::project(const Day start, const Day ending) {
    update_if_changed();
    Day day = start;
    while (day <= ending) {
        (void)get_or_project(day);
        day = day.next_month();
    }
}

void Accounts::update_if_changed() const {
    return_if(!changed_);
    changed_ = false;
    dates_.clear();
    std::set<Day> days;
    for (const std::string_view &group : groups_) {
        for (const auto &account : accounts_.at(group)) {
            const auto account_dates = account->dates();
            days.insert(account_dates.begin(), account_dates.end());
        }
    }
    // Populate all registered days so far.
    for (const Day &day : days) {
        (void)get_or_project(day);
    }
}

Account &Accounts::add(const std::shared_ptr<Account> &account) {
    static constexpr std::vector<std::shared_ptr<Account>> kEmpty = {};
    changed_ = true;
    const auto [iter, inserted] = accounts_.try_emplace(account->category(), kEmpty);
    if (inserted) {
        groups_.push_back(account->category());
    }
    return *iter->second.emplace_back(account);
}

Event &Accounts::add(const std::shared_ptr<Event> &event) {
    return *events_.emplace_back(event);
}
