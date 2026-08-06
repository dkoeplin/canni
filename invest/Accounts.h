#pragma once

#include <string_view>

#include "invest/account/Account.h"
#include "invest/event/Event.h"
#include "invest/Day.h"
#include "invest/Taxes.h"
#include "invest/USD.h"

/**
 * @class Accounts
 * @brief Holds groups of accounts grouped by categories
 */
class Accounts {
  public:
    Accounts() = default;

    /// Creates an account, owned by this object, and returns a reference to that account.
    template <typename T, typename ...Args>
        requires std::is_base_of_v<Account, T> || std::is_base_of_v<Event, T>
    T &add(Args... args) { return *static_cast<T *>(&add(std::make_shared<T>(args...))); }

    /// Returns the group balances on the given day, or their projections if any entry for the day did not exist.
    std::vector<USD> operator[](const Day &date) const;

    /// Returns the "cumulative" total through this group, in order, on the given date.
    USD cumulative(const std::string_view &group, const Day &date) const;

    /// Returns the total balance across all groups on the given day, or a projection if the day did not exist.
    USD total(const Day &date) const;

    /// Returns total balances, by category, on the given day, or a projection if the day did not exist.
    const std::vector<USD> &totals(const Day &date) const;

    std::vector<std::pair<std::string_view, USD>> tagged_totals(const Day &date) const;

    /// Returns an ordered iterator over all dates with registered or projected balances.
    auto dates() const {
        update_if_changed();
        return dates_ | std::views::keys;
    }

    /// Add monthly projections for a specified range of months from [start] to [end].
    void project(Day start, Day end);

    pure auto begin() const { return groups_.begin(); }
    pure auto end() const { return groups_.end(); }
    pure auto rbegin() const { return groups_.rbegin(); }
    pure auto rend() const { return groups_.rend(); }

    /// Returns a vector of all accounts of type T.
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
    Account &add(const std::shared_ptr<Account>& account);
    Event &add(const std::shared_ptr<Event> &event);
    const std::vector<USD> &get_or_project(const Day &date) const;

    /// Recompute from all available dates in all accounts.
    void update_if_changed() const;

    mutable bool changed_ = false;
    mutable std::map<Day, std::vector<USD>> dates_;              // Memoized per-group totals for queried days
    std::vector<std::string_view> groups_;
    std::unordered_map<std::string_view, std::vector<std::shared_ptr<Account>>> accounts_;  // All accounts
    std::vector<std::shared_ptr<Event>> events_;                                            // All events
};