#pragma once

#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "canni/account/Account.h"
#include "canni/account/MarketReturns.h"
#include "canni/account/Taxes.h"
#include "canni/data/Day.h"
#include "canni/data/Inflation.h"
#include "canni/data/USD.h"
#include "canni/event/Event.h"
#include "canni/plot/Series.h"

namespace canni {

class Company;
struct Scenario;

abstract struct Portfolio {
    struct History {
        std::vector<Day> dates;
        std::vector<std::vector<Entry>> rows;
    };

    struct Params {
        /// Market assumptions
        Interest avg_inflation_rate;

        /// Important dates
        Day birth;  // Birth date of user
        Day today;  // Current date
        Day retire; // Retirement date
        Day ending; // Last date for projections

        /// Simulation controls
        std::optional<MarketReturns> returns = std::nullopt;
        std::optional<History> history = std::nullopt;
        bool verbose = false;
    };

    explicit Portfolio(const Params &params, const Scenario &scenario);

    virtual ~Portfolio() = default;

    /// Captures current date/balance history for later restoration via load_history().
    pure History snapshot() const { return {dates_, rows_}; }

    /// Replaces date/balance history with a previously captured snapshot.
    void load_history(const History &h) { dates_ = h.dates; rows_ = h.rows; }

    struct ColumnType {
        enum Type { kIgnore, kBalance, kPrincipal };
        constexpr explicit ColumnType(Account *account, Type type) : account(account), type(type) {}
        Account *account;
        Type type;
    };
    static const ColumnType Ignore;
    static ColumnType Balance(Account &account) { return ColumnType(&account, ColumnType::kBalance); }
    static ColumnType Principal(Account &account) { return ColumnType(&account, ColumnType::kPrincipal); }

    /// Parses account history from CSV [filename], with the specified columns.
    /// The first column is assumed to be the date, while others are specified by the column types above.
    void import_csv(const std::string &filename, const std::vector<ColumnType> &columns);

    /// Creates an account or event, owned by this object, and returns a reference to it.
    template <typename T, typename ...Args>
        requires std::is_base_of_v<Account, T> || std::is_base_of_v<Event, T>
    T &add(Args... args) { return *static_cast<T *>(&add(std::make_shared<T>(this, args...))); }

    /// Returns the total sums for each day, including the breakdown by group with each total.
    /// If [normalize] is true, presents all balances in today's dollars rather than raw balances.
    pure Series<Day, USD> totals(const std::string &name, bool normalize = true) const;

    /// Returns the totals for each group, ordered by the original group registration order.
    /// If [normalize] is true, presents all balances in today's dollars rather than raw balances.
    pure std::vector<Series<Day, USD>> grouped_totals(bool normalize = true) const;

    /// Returns the current total across all accounts.
    pure USD total() const;

    /// Returns a reference to the latest balance for this account.
    /// Asserts that this account is actually registered with this parent.
    Entry &current_entry(const Account *account);

    pure const Day &current_day() const { return dates_.back(); }
    pure const Day &ending_day() const { return params_.ending; }

    /// Seeds the initial state with a starting date and zero balances for all registered accounts.
    /// Must be called before project() when not using import_csv().
    void seed(const Day &day);

    /// Projects [step] from the current ending date using current account balances and registered events.
    Portfolio &project_once(Day::Distance step);

    /// Projects from the current ending date through [last] at a step of [step].
    Portfolio &project_until(Day last, const Day::Distance &step = 1_months);

    Portfolio &project(const Day::Distance &step = 1_months) { return project_until(params_.ending, step); }

    template <typename T>
    struct const_iterator : std::vector<Account *>::const_iterator {
        using pointer = const T *;
        using reference = const T &;
        using parent = std::vector<Account *>::const_iterator;
        explicit const_iterator(const parent iter) : parent(iter) {}
        reference operator*() const { return *static_cast<T *const>(parent::operator*()); }
        pointer operator->() const { return static_cast<T *const>(*parent::operator->()); }
    };
    template <typename T>
    struct const_range {
        explicit const_range(const std::vector<Account *> &accounts) : accounts_(accounts) {}
        const_iterator<T> begin() const { return const_iterator<T>(accounts_.begin()); }
        const_iterator<T> end() const { return const_iterator<T>(accounts_.end()); }

        pure USD balance() const {
            USD total;
            for (const auto &account : *this)
                total += account.balance();
            return total;
        }

        const std::vector<Account *> &accounts_;
    };

    /// Returns all accounts of type T.
    template <typename T>
        requires std::is_base_of_v<Account, T>
    pure const_range<T> get() const {
        return const_range<T>(accounts_by_group_.at(T::kKind));
    }

    pure const MarketReturns *market_returns() const { return params_.returns ? &*params_.returns : nullptr; }

    /// Sells [n] vested options for [price] on [day].
    void sell_vested_options(const Company &company, const Day &day, I64 n, USD price);

    virtual void deposit(USD amount) const = 0;

    pure USD withdraw(const std::string &name, USD amount);

    void note(const std::string &event);

    pure std::vector<Series<I64, USD>> grouped_expenses() const;

    void print_options(Day day, USD value) const;

    /// In a given simulated cycle, determines how to move balances between accounts after all other events.
    virtual void invest() const = 0;

    virtual bool solvent() const = 0;

    Taxes taxes;
    Inflation inflation;

  protected:
    Account &add(const std::shared_ptr<Account> &account);
    Event &add(const std::shared_ptr<Event> &event);

    template <typename T>
    bool out(USD &withdrawn, USD &amount) {
        const auto accounts = get<T>();
        auto iter = accounts.begin();
        USD total;
        while (amount > 0.00_USD && iter != accounts.end()) {
            const auto out = iter->withdraw(amount);
            total += out;
            amount -= out;
            ++iter;
        }
        withdrawn += total;
        if (amount <= 0.00_USD) return true;
        return false;
    }

    Params params_;

    std::vector<std::shared_ptr<Account>> accounts_;
    std::vector<std::vector<Account *>> accounts_by_group_;
    std::vector<std::shared_ptr<Event>> events_;

    std::vector<Day> dates_;
    std::vector<std::vector<Entry>> rows_;

    mutable std::unordered_map<I64, USD> expenses_;
    mutable std::unordered_map<I64, std::map<std::string, USD>> expenses_breakdown_;
    std::unordered_map<I64, std::vector<std::string>> notes_; // Recorded events per year
};

template <typename P>
concept PortfolioSubclass = std::derived_from<P, Portfolio> && requires (typename P::Params p, const Scenario &mod) {
    { P(p, mod) };                                          // Constructed from params and Scenario modifier
    { p.base } -> std::convertible_to<Portfolio::Params &>; // Includes a Portfolio::Params field.
};

} // namespace canni
