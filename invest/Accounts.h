#pragma once

#include <string_view>
#include <vector>

#include "invest/account/Account.h"
#include "invest/event/Event.h"
#include "invest/Day.h"
#include "invest/Taxes.h"
#include "invest/USD.h"

class Accounts {
  public:
    // Snapshot of all group totals for one day. Immutable once appended.
    struct Row {
        Day date;
        std::vector<USD> group_totals;  // one per group, in groups_ order
    };

    Accounts() = default;

    /// Creates an account or event, owned by this object, and returns a reference to it.
    template <typename T, typename ...Args>
        requires std::is_base_of_v<Account, T> || std::is_base_of_v<Event, T>
    T &add(Args... args) { return *static_cast<T *>(&add(std::make_shared<T>(args...))); }

    /// Returns the group totals on [date], or the closest row at or before [date].
    std::vector<USD> operator[](const Day &date) const;

    /// Returns the cumulative total through [group] on [date].
    USD cumulative(const std::string_view &group, const Day &date) const;

    /// Returns the total balance across all groups on [date].
    USD total(const Day &date) const;

    /// Returns totals by group on [date].
    const std::vector<USD> &totals(const Day &date) const;

    std::vector<std::pair<std::string_view, USD>> tagged_totals(const Day &date) const;

    /// Returns dates of all stored rows.
    auto dates() const { return rows_ | std::views::transform([](const Row &r) { return r.date; }); }

    /// Runs month-by-month projection from [start] to [end], building one Row per month.
    void project(Day start, Day end);

    pure auto begin() const { return groups_.begin(); }
    pure auto end() const { return groups_.end(); }
    pure auto rbegin() const { return groups_.rbegin(); }
    pure auto rend() const { return groups_.rend(); }

    /// Returns all accounts of type T.
    template <typename T>
        requires std::is_base_of_v<Account, T>
    pure std::vector<T *> get() const {
        static const std::vector<T *> kEmpty;
        const std::string_view category = nvl::reflect<T>().name;
        const auto iter = accounts_.find(category);
        return_if(iter == accounts_.end(), kEmpty);
        std::vector<T *> accounts;
        for (const auto &account : iter->second) {
            accounts.push_back(nvl::dyn_cast<T>(account.get()));
        }
        return accounts;
    }

    mutable Taxes taxes;

  private:
    Account &add(const std::shared_ptr<Account> &account);
    Event &add(const std::shared_ptr<Event> &event);

    /// Returns the stored row closest to [date] (at or before). Asserts rows_ is non-empty.
    const std::vector<USD> &find_row(const Day &date) const;

    std::vector<Row> rows_;
    std::vector<std::string_view> groups_;
    std::unordered_map<std::string_view, std::vector<std::shared_ptr<Account>>> accounts_;
    std::vector<std::shared_ptr<Event>> events_;
};
