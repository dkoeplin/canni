#pragma once

#include <functional>
#include <map>

#include "Day.h"
#include "USD.h"

class Inflation;

class Taxes {
  public:
    struct Paid {
        USD standard;         // Taxes already paid on standard income (e.g. for salary)
        USD medicare;         // Medicare taxes already paid
        USD social_security;  // Social security taxes already paid

        pure USD total() const { return standard + medicare + social_security; }
    };
    struct Income {
        std::map<std::string, USD> std_income;

        USD standard;          // Standard income for the year
        USD dividends;         // Qualified dividends
        USD short_term_gains;  // Short term capital gains
        USD long_term_gains;   // Long term capital gains

        pure USD total() const { return standard + dividends + short_term_gains + long_term_gains; }
    };
    struct Deductions {
        USD donations;         // Donations made this year.
        USD mortgage;          // Total mortgage interest paid in the year.
        USD salt;              // State And Local Taxes (SALT). Primarily property tax in NH. Capped at 10K.

        /// Returns the total itemized deductions. Does not include withholding - this is not a true deduction.
        pure USD total() const { return donations + mortgage + std::min(10000_USD, salt); }
    };
    struct Entry {
        Income income;
        Deductions deductions;
        Paid paid;
    };

    pure Entry get(const Day &day) const;

    Taxes &with(const Day &day, const USD &amount, const std::function<USD &(Entry &)>& func);

    /// Mark tax deductions during the year.
    Taxes &donation(const Day &day, const USD &amount) {
        return with(day, amount, [](Entry &b) -> USD& { return b.deductions.donations; });
    }
    Taxes &mortgage(const Day &day, const USD &amount) {
        return with(day, amount, [](Entry &b) -> USD& { return b.deductions.mortgage; });
    }
    Taxes &property_tax(const Day &day, const USD &amount) {
        return with(day, amount, [](Entry &b) -> USD& { return b.deductions.salt; });
    }

    /// Mark paid taxes during the year.
    Taxes &withholding(const Day &day, const USD &amount) {
        return with(day, amount, [](Entry &b) -> USD& { return b.paid.standard; });
    }
    Taxes &medicare(const Day &day, const USD &amount ) {
        return with(day, amount, [](Entry &b) -> USD& { return b.paid.medicare; });
    }
    Taxes &social_security(const Day &day, const USD &amount) {
        return with(day, amount, [](Entry &b) -> USD& { return b.paid.social_security; });
    }

    /// Mark various types of income during the year.
    Taxes &income(const std::string &name, const Day &day, const USD &amount) {
        with(day, amount, [](Entry &b)-> USD& { return b.income.standard; });
        with(day, amount, [&](Entry &b)-> USD& { return b.income.std_income[name]; });
        return *this;
    }
    Taxes &dividends(const Day &day, const USD &amount) {
        return with(day, amount, [](Entry &b)-> USD& { return b.income.dividends; });
    }
    Taxes &short_term(const Day &day, const USD &amount) {
        return with(day, amount, [](Entry &b)-> USD& { return b.income.short_term_gains; });
    }
    Taxes &long_term(const Day &day, const USD &amount) {
        return with(day, amount, [](Entry &b)-> USD& { return b.income.long_term_gains; });
    }

  private:
    mutable std::map<Day, Entry> value_;  /// Recorded tax breakdown, by date.
};

struct CalculatedTaxes {
    I64 year;
    USD federal_income;   // Regular income tax on ordinary income + STCG
    USD ltcg;             // Tax on long-term gains + qualified dividends
    USD niit;             // Net Investment Income Tax (3.8%)
    USD medicare;         // Medicare (1.45% base + 0.9% above $250K MFJ threshold)
    USD social_security;  // Social Security (6.2% up to wage base)
    Taxes::Entry entry;   // Corresponding entry

    pure USD total() const { return federal_income + ltcg + niit + medicare + social_security; }
    pure USD net_owed() const { return total() - entry.paid.total(); }
};
CalculatedTaxes calculate_taxes(double avg_inflation, const Taxes &taxes, I64 year);

inline std::ostream &operator<<(std::ostream &os, const CalculatedTaxes &taxes) {
    os << "=============== " << taxes.year << " Tax Summary ===============" << std::endl;
    os << "=== Income ===" << std::endl
       << "            Standard:  " << taxes.entry.income.standard << std::endl;
    for (const auto &[name, total] : taxes.entry.income.std_income) {
        if (total > 0.00_USD) {
            os << "                  " << name << ": " << total << std::endl;
        }
    }
    os << "            Dividends: " << taxes.entry.income.dividends << std::endl
       << "Capital Gains (short): " << taxes.entry.income.short_term_gains << std::endl
       << " Capital Gains (long): " << taxes.entry.income.long_term_gains << std::endl;
    os << "=== Deductions ===" << std::endl
       << "            Donations: " << taxes.entry.deductions.donations << std::endl
       << "             Mortgage: " << taxes.entry.deductions.mortgage << std::endl
       << "                 SALT: " << std::min(taxes.entry.deductions.salt, 10000_USD) << std::endl;
    os << "=== Total Taxes ===" << std::endl
       << "          Income tax: " << taxes.federal_income << std::endl
       << "                     -" << taxes.entry.paid.standard << std::endl
       << "Capital Gains (long): " << taxes.ltcg << std::endl
       << "                NIIT: " << taxes.niit << std::endl
       << "            Medicare: " << taxes.medicare << std::endl
       << "                     -" << taxes.entry.paid.medicare << std::endl
       << "     Social Security: " << taxes.social_security << std::endl
       << "                     -" << taxes.entry.paid.social_security << std::endl
       << "  Total: " << taxes.total() << std::endl
       << "   Owed: " << taxes.net_owed() << std::endl;
    return os;
}