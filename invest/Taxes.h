#pragma once

#include <functional>
#include <vector>

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

    pure const Entry &get(const Day &day) const;

    /// Mark tax deductions during the year.
    void donation(const Day &day, const USD &amount) { entry_for(day.year()).deductions.donations += amount; }
    void mortgage(const Day &day, const USD &amount) { entry_for(day.year()).deductions.mortgage += amount; }
    void property_tax(const Day &day, const USD &amount) { entry_for(day.year()).deductions.salt += amount; }

    /// Mark paid taxes during the year.
    void withholding(const Day &day, const USD &amount) { entry_for(day.year()).paid.standard += amount; }
    void medicare(const Day &day, const USD &amount ) { entry_for(day.year()).paid.medicare += amount; }
    void social_security(const Day &day, const USD &amount) { entry_for(day.year()).paid.social_security += amount; }

    /// Mark various types of income during the year.
    void income(const Day &day, const USD &amount) { entry_for(day.year()).income.standard += amount; }
    void dividends(const Day &day, const USD &amount) { entry_for(day.year()).income.dividends += amount; }
    void short_term(const Day &day, const USD &amount) { entry_for(day.year()).income.short_term_gains += amount; }
    void long_term(const Day &day, const USD &amount) { entry_for(day.year()).income.long_term_gains += amount; }

  private:
    Entry &entry_for(I64 year) {
        if (entries_.empty()) {
            base_year_ = year;
        } else if (year < base_year_) {
            entries_.insert(entries_.begin(), base_year_ - year, Entry{});
            base_year_ = year;
        }
        if (year - base_year_ >= static_cast<I64>(entries_.size())) {
            entries_.resize(year - base_year_ + 1);
        }
        return entries_[year - base_year_];
    }

    I64 base_year_ = 0;
    std::vector<Entry> entries_;
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
CalculatedTaxes calculate_taxes(const Inflation &inflation, const Taxes &taxes, I64 year);

inline std::ostream &operator<<(std::ostream &os, const CalculatedTaxes &taxes) {
    os << "=============== " << taxes.year << " Tax Summary ===============" << std::endl;
    os << "=== Income ===" << std::endl
       << "            Standard:  " << taxes.entry.income.standard << std::endl;
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