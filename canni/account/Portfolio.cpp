#include "Portfolio.h"

#include <cmath>
#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Options.h"
#include "Scenario.h"
#include "canni/data/Inflation.h"
#include "canni/data/Parsing.h"

namespace canni {

Portfolio::Portfolio(const Params &params, const Scenario &scenario)
    : inflation(this, params.today, params.avg_inflation_rate), params_(params) {

    if (params_.history) {
        load_history(*params_.history);
    }

    /// Force the IRS required minimum distribution from pre-tax retirement accounts each year.
    /// Fires at year-end; only withdraws the remainder not already taken via normal spending.
    add<Yearly>("RMD", Day(1, Month::Dec, params_.birth.year() + 73), [this](Day d) {
        const I64 age = d.year() - params_.birth.year();
        const double factor = Taxes::required_minimum_distribution(age);
        return_if(factor <= 0.0);

        USD balance;
        USD withdrawn = taxes.get(d).retirement_withdrawals;
        const auto accounts = get<Retirement>();
        for (const Retirement &a : accounts) {
            if (a.type() == Retirement::kPreTax) {
                balance += a.balance();
            }
        }
        const USD required = balance * (1.0 / factor); // Minimum required distribution
        USD remainder = required - withdrawn;          // Remaining required withdrawals
        for (auto &account : accounts) {
            return_if(remainder <= 0_USD);
            const USD actual = account.withdraw(remainder);
            deposit(actual);
            remainder -= actual;
        }
    });

    // Apply modifier (adds housing events, sale events, donations, etc.) before the sweep.
    if (scenario.apply) scenario.apply(*this);
}

const Portfolio::ColumnType Portfolio::Ignore = ColumnType(nullptr, ColumnType::kIgnore);

void Portfolio::import_csv(const std::string &filename, const std::vector<ColumnType> &columns) {
    const auto data = parse_data(filename);
    for (const auto &i : data) {
        if (const auto day = Day::parse(i[0])) {
            dates_.push_back(*day);
            for (const auto &account : accounts_) {
                rows_[account->index()].push_back(account->estimate(*day));
            }
            for (I64 j = 0; j < static_cast<I64>(columns.size()); ++j) {
                const auto &col = columns[j];
                if (col.type == ColumnType::kIgnore) {
                    // Skip
                } else if (col.type == ColumnType::kPrincipal) {
                    rows_[col.account->index()].back().principal = USD::parse(i[j + 1]);
                } else if (col.type == ColumnType::kBalance) {
                    rows_[col.account->index()].back().balance = USD::parse(i[j + 1]);
                }
            }
        }
    }
}

Series<Day, USD> Portfolio::totals(const std::string &name, const bool normalize) const {
    Series<Day, USD> series;
    series.name = name;
    for (U64 i = 0; i < dates_.size(); ++i) {
        const Day &day = dates_.at(i);
        USD total;
        for (U64 g = 0; g < Account::Kind::kNUM_ACCOUNT_TYPES; ++g) {
            if (g != Account::kRealEstate && g != Account::kOptions) {
                for (const auto &account : accounts_by_group_.at(g)) {
                    auto balance = rows_.at(account->index()).at(i).balance;
                    total += normalize ? inflation.inverse(day, balance) : balance;
                }
            }
        }
        series.points.emplace_back(day, total);
    }
    return series;
}

std::vector<Series<Day, USD>> Portfolio::grouped_totals(const bool normalize) const {
    std::vector<Series<Day, USD>> groups;
    for (U64 g = 0; g < Account::kNUM_ACCOUNT_TYPES; ++g) {
        Series<Day, USD> series;
        series.name = to_string(static_cast<Account::Kind>(g));
        groups.push_back(series);
    }
    for (U64 i = 0; i < dates_.size(); ++i) {
        const auto &day = dates_.at(i);
        for (U64 g = 0; g < Account::kNUM_ACCOUNT_TYPES; ++g) {
            USD group_total;
            for (const auto &account : accounts_by_group_.at(g)) {
                const auto balance = rows_.at(account->index()).at(i).balance;
                group_total += normalize ? inflation.inverse(day, balance) : balance;
            }
            groups[g].points.emplace_back(day, group_total);
        }
    }
    return groups;
}

USD Portfolio::total() const {
    USD total;
    for (const auto &account : accounts_) {
        total += account->balance();
    }
    return total;
}

Entry &Portfolio::current_entry(const Account *account) {
    ASSERT(account->index() < rows_.size(), "No entry for account " << account->name());
    auto &row = rows_.at(account->index());
    ASSERT(!row.empty(), "No entries defined for account " << account->name());
    return row.back();
}

void Portfolio::seed(const Day &day) {
    dates_.push_back(day);
    for (const auto &account : accounts_) {
        rows_[account->index()].push_back(account->estimate(day));
    }
}

Portfolio &Portfolio::project_once(const Day::Distance step) {
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

    // Move balances, e.g. from cash to investments
    invest();

    return *this;
}

Portfolio &Portfolio::project_until(const Day last, const Day::Distance &step) {
    ASSERT(!dates_.empty(), "Accounts::project_until requires account history.");
    while (dates_.back() < last) {
        project_once(step);
    }
    return *this;
}

Account &Portfolio::add(const std::shared_ptr<Account> &account) {
    static const std::vector<Entry> kEmptyEntries = {};

    const auto &ref = accounts_.emplace_back(account);
    const U64 idx = accounts_.size() - 1;
    ref->set_index(idx);

    accounts_by_group_.resize(Account::kNUM_ACCOUNT_TYPES);
    accounts_by_group_[account->kind()].push_back(ref.get());

    if (params_.history) {
        const auto &snaps = params_.history->account_snapshots;
        if (idx < snaps.size() && snaps[idx])
            ref->restore(*snaps[idx]);
    }

    // Ensure the new account has a full history to date
    auto &row = rows_.emplace_back(kEmptyEntries);
    while (row.size() < dates_.size()) {
        const auto day = dates_.at(row.size());
        row.push_back(account->estimate(day));
    }

    return *ref;
}

Event &Portfolio::add(const std::shared_ptr<Event> &event) {
    return *events_.emplace_back(event);
}

void Portfolio::sell_vested_options(const Company &company, const Day &day, const I64 n, const USD price) {
    if (params_.verbose) {
        std::cout << "-----------------------------------------------\n"
                  << "[" << day << "] Selling " << n << " vested options at " << price << ":\n";
    }
    Options::Sale sale;
    I64 remain = n;
    auto options_list = company.options();
    auto iter = options_list.begin();
    while (remain > 0 && iter != options_list.end()) {
        Options *options = *iter;
        const Options::Count total = options->vested(day) - options->sold(day);
        Options::Count count;
        count.nso = std::min(remain, total.nso);
        remain -= count.nso;
        count.iso = std::min(remain, total.iso);
        remain -= count.iso;
        const auto inst = options->sell(day, price, count, true);
        ++iter;
        if (params_.verbose)
            std::cout << "  " << options->name() << ":" << count
                      << "\n    Exercise: " << inst.costs
                      << "\n    Proceeds: " << inst.proceeds << "\n";
        sale += inst;
    }
    if (params_.verbose)
        std::cout << "Total:\n  Exercise: " << sale.costs << "\n  Proceeds: " << sale.proceeds
                  << "\n-----------------------------------------------\n";
    (void)withdraw("Options exercise on " + day.to_string(), sale.costs);
    deposit(sale.proceeds);
}

std::vector<Series<I64, USD>> Portfolio::grouped_expenses() const {
    std::vector<Series<I64, USD>> result;
    std::unordered_set<std::string> seen;
    for (const auto &exps : expenses_breakdown_ | std::views::values)
        for (const auto &cat : exps | std::views::keys)
            if (seen.insert(cat).second)
                result.emplace_back().name = cat;
    for (const auto &[year, exps] : expenses_breakdown_) {
        for (auto &series : result) {
            const auto it = exps.find(series.name);
            const auto exp = it == exps.end() ? 0_USD : inflation.inverse(Day(31, Month::Dec, year), it->second);
            series.points.emplace_back(year, exp);
        }
    }
    return result;
}

pure USD Portfolio::withdraw(const std::string &name, USD amount) {
    const auto &today = current_day();
    if (params_.verbose) {
        expenses_[today.year()] += amount;
        expenses_breakdown_[today.year()][name] += amount;
    }
    const bool retirement_age = today > (params_.birth + 59_years + 6_months);
    USD withdrawn;
    return_if(out<Cash>(withdrawn, amount), withdrawn);
    if (retirement_age) return_if(out<Retirement>(withdrawn, amount), withdrawn);
    return_if(out<Bonds>(withdrawn, amount), withdrawn);
    return_if(out<Stocks>(withdrawn, amount), withdrawn);
    return withdrawn;
}

void Portfolio::note(const std::string &event) {
    if (params_.verbose) {
        const auto &today = current_day();
        notes_[today.year()].push_back("[" + today.to_string() + "] " + event);
    }
}

} // namespace canni
