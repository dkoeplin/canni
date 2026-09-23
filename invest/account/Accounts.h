#pragma once

#include <optional>
#include <random>
#include <string_view>
#include <vector>

#include "invest/Day.h"
#include "invest/account/Account.h"
#include "invest/event/Event.h"
#include "invest/Day.h"
#include "invest/Inflation.h"
#include "invest/Taxes.h"
#include "invest/USD.h"
#include "invest/Series.h"

struct MarketReturns {
  public:
    struct AssetClass {
        AssetType type; // Asset type
        double mean;    // Expected annual return (e.g. 0.07)
        double sigma;   // Annual standard deviation (e.g. 0.15)
    };

    /// Preallocates [years] annual returns for each asset class via log-normal sampling.
    MarketReturns(I64 base_year, I64 years, const std::vector<AssetClass> &classes, std::mt19937 &rng);

    pure double at(AssetType type, I64 year) const;
    pure double cumulative(AssetType type, I64 year) const;

  private:
    I64 base_year_ = 0;
    std::vector<std::vector<double>> classes_;
    std::vector<std::vector<double>> cumulative_;
};


struct Accounts {
    Accounts() = default;

    struct History {
        std::vector<Day> dates;
        std::vector<std::vector<Entry>> rows;
    };

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
    /// If [inflation] is provided, presents all balances in today's dollars rather than raw balances.
    pure Series<Day, USD> totals(const std::string &name, std::optional<Inflation> inflation) const;

    /// Returns the totals for each group, ordered by the original group registration order.
    /// If [inflation] is provided, presents all balances in today's dollars rather than raw balances.
    pure std::vector<Series<Day, USD>> grouped_totals(std::optional<Inflation> inflation) const;

    /// Returns the current total across all accounts.
    pure USD total() const;

    /// Returns a reference to the latest balance for this account.
    /// Asserts that this account is actually registered with this parent.
    Entry &current_entry(const Account *account);

    pure Day current_day() const { return dates_.back(); }

    /// Seeds the initial state with a starting date and zero balances for all registered accounts.
    /// Must be called before project() when not using import_csv().
    void seed(const Day &day);

    /// Projects [step] from the current ending date using current account balances and registered events.
    Accounts &project(Day::Distance step);

    /// Projects from the current ending date through [last] at a step of [step].
    Accounts &project_until(const Day &last, const Day::Distance &step = 1_months);

    /// Iterators over groups names.
    pure auto begin() const { return groups_.begin(); }
    pure auto end() const { return groups_.end(); }
    pure auto rbegin() const { return groups_.rbegin(); }
    pure auto rend() const { return groups_.rend(); }

    /// Returns all accounts of type T.
    template <typename T>
        requires std::is_base_of_v<Account, T>
    pure const std::vector<Account *> &get() const {
        static constexpr std::vector<Account *> kEmpty;
        const std::string_view category = nvl::reflect<T>().name;
        const auto iter = accounts_by_group_.find(category);
        return iter != accounts_by_group_.end() ? iter->second : kEmpty;
    }

    void set_market_returns(MarketReturns returns) { market_returns_ = std::move(returns); }
    pure const MarketReturns *market_returns() const { return market_returns_ ? &*market_returns_ : nullptr; }

    Taxes taxes;

  private:
    Account &add(const std::shared_ptr<Account> &account);
    Event &add(const std::shared_ptr<Event> &event);

    std::vector<std::string_view> groups_;
    std::vector<std::shared_ptr<Account>> accounts_;
    std::unordered_map<std::string_view, std::vector<Account *>> accounts_by_group_;
    std::vector<std::shared_ptr<Event>> events_;

    std::optional<MarketReturns> market_returns_;
    std::vector<Day> dates_;
    std::vector<std::vector<Entry>> rows_;
};
