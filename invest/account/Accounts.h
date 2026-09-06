#pragma once

#include <string_view>
#include <vector>

#include "invest/Day.h"
#include "invest/account/Account.h"
#include "invest/event/Event.h"
#include "invest/Day.h"
#include "invest/Taxes.h"
#include "invest/USD.h"


struct Accounts {
    Accounts() = default;

    struct ColumnType {
        enum Type { kIgnore, kBalance, kPrincipal };
        constexpr explicit ColumnType(Account *account, Type type) : account(account), type(type) {}
        Account *account;
        Type type;
    };
    static const ColumnType Ignore;
    static ColumnType Balance(Account &account) { return ColumnType(&account, ColumnType::kBalance); }
    static ColumnType Principal(Account &account) { return ColumnType(&account, ColumnType::kPrincipal); }

    /// Parse account history from [filename], with the specified columns.
    /// The first column is assumed to be the date, while others are specified by the column types above.
    void parse_history(const std::string &filename, const std::vector<ColumnType> &columns);

    /// Creates an account or event, owned by this object, and returns a reference to it.
    template <typename T, typename ...Args>
        requires std::is_base_of_v<Account, T> || std::is_base_of_v<Event, T>
    T &add(Args... args) { return *static_cast<T *>(&add(std::make_shared<T>(this, args...))); }

    struct GroupTotals {
        pure USD cumulative(std::string_view group) const;

        Day day;
        USD total;
        std::vector<std::pair<std::string_view, USD>> groups;
    };
    /// Returns the total sums for each day, including the breakdown by group with each total.
    pure std::vector<GroupTotals> totals() const;

    /// Returns the current total across all accounts.
    pure USD total() const;

    /// Returns a reference to the latest balance for this account.
    /// Asserts that this account is actually registered with this parent.
    Entry &current_entry(const Account *account);

    pure Day current_day() const { return dates_.back(); }

    /// Projects [step] from the current ending date using current account balances and registered events.
    void project(Day::Distance step);

    /// Projects from the current ending date through [last] at a step of [step].
    void project_until(const Day &last, const Day::Distance &step = 1_months);

    /// Iterators over groups names.
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
        const auto iter = accounts_by_group_.find(category);
        return_if(iter == accounts_by_group_.end(), kEmpty);
        std::vector<T *> accounts;
        for (const auto &account : iter->second) {
            accounts.push_back(nvl::dyn_cast<T>(account));
        }
        return accounts;
    }

    Taxes taxes;

  private:
    Account &add(const std::shared_ptr<Account> &account);
    Event &add(const std::shared_ptr<Event> &event);

    std::vector<std::string_view> groups_;
    std::vector<std::shared_ptr<Account>> accounts_;
    std::unordered_map<std::string_view, std::vector<Account *>> accounts_by_group_;
    std::vector<std::shared_ptr<Event>> events_;

    std::vector<Day> dates_;
    std::unordered_map<const Account *, std::vector<Entry>> rows_;
};
