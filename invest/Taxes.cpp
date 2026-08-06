#include "Taxes.h"

#include "invest/Inflation.h"

Taxes &Taxes::with(const Day &day, const USD &amount, const std::function<USD &(Entry &)> &func) {
    const auto prev = get(day);
    const auto [iter, _] = value_.try_emplace(day, prev); // Add explicit entry for this day if it didn't exist
    for (auto i = iter; i != value_.end() && i->first.year() == day.year(); ++i) { // Increment every entry in this year
        USD &field = func(i->second);
        field += amount;
    }
    return *this;
}

Taxes::Entry Taxes::get(const Day& day) const {
    const auto it = value_.upper_bound(day); // Strictly after this day
    return_if(it == value_.begin() || value_.empty(), {});
    const auto iter = std::prev(it);
    return_if(iter->first.year() != day.year(), {}); // Stay within the same calendar year
    return iter->second;
}

namespace {

struct TaxBracket {
    USD max;
    double rate;
};

const std::vector<TaxBracket> k2026TaxBrackets = {
    { 24800_USD, 0.10},
    {100800_USD, 0.12},
    {211400_USD, 0.22},
    {403550_USD, 0.24},
    {512450_USD, 0.32},
    {768700_USD, 0.35},
    {USD::dollars(INT32_MAX), 0.37}
};

USD income_tax(const Inflation &inflation,
               const Day &date,
               const USD &income,
               const std::vector<TaxBracket> &brackets = k2026TaxBrackets) {
    USD total;
    const U64 N = brackets.size();
    for (U64 i = 0; i < N; ++i) {
        const TaxBracket &bracket = brackets[i];
        const USD min = i == 0 ? 0.00_USD : inflation(date, brackets[i - 1].max);
        const USD max = inflation(date, bracket.max);
        total += std::max(0.00_USD, std::min(max, income) - min) * bracket.rate;
    }
    return total;
}

// MFJ 2026 LTCG rate thresholds — based on total taxable income including preferential income
// Verify against IRS Rev. Proc. for the year
const std::vector<TaxBracket> k2026LongTermCapGainsBracketsMFJ = {
    {100800_USD,              0.00},
    {583750_USD,              0.15},
    {USD::dollars(INT32_MAX), 0.20},
};

// Qualified dividends and LTCG are stacked on top of ordinary taxable income to find the rate.
USD long_term_capital_gains_tax(const Inflation &inflation, const Day &date,
                                const USD &ordinary_taxable, const USD &preferential) {
    USD tax;
    USD stacked = ordinary_taxable;
    USD remaining = preferential;
    for (const auto &[max_income, rate] : k2026LongTermCapGainsBracketsMFJ) {
        if (remaining <= 0.00_USD) break;
        const USD max = inflation(date, max_income);
        const USD room = std::max(0.00_USD, max - stacked);
        const USD in_bracket = std::min(remaining, room);
        tax += in_bracket * rate;
        stacked += in_bracket;
        remaining -= in_bracket;
    }
    return tax;
}

/// Returns the total Net Investment Income Tax (NIIT) owed based on the income breakdown.
/// Threshold uses pre-deduction MAGI; tax base is investment income only (not wages).
USD niit_tax(const Inflation &, const Day &, const Taxes::Entry &entry) {
    // NOT historically an inflation adjusted threshold
    static const auto kNIITThreshold = 250000_USD;

    const USD magi = entry.income.standard + entry.income.short_term_gains
                   + entry.income.dividends + entry.income.long_term_gains;

    return_if(magi <= kNIITThreshold, 0.00_USD);
    const USD nii = entry.income.short_term_gains + entry.income.dividends + entry.income.long_term_gains;
    return std::min(nii, magi - kNIITThreshold) * 0.038;
}

/// Returns the total Medicare tax owed based on the income breakdown.
/// Base rate 1.45% on wages; additional 0.9% on wages above MFJ threshold.
USD medicare(const Inflation &, const Day &, const Taxes::Entry &entry) {
    // NOT historically an inflation adjusted threshold
    static const auto kMedicareAdditionalThreshold = 250000_USD;

    const USD wages = entry.income.standard;
    USD tax = wages * 0.0145;
    if (wages > kMedicareAdditionalThreshold) {
        tax += (wages - kMedicareAdditionalThreshold) * 0.009;
    }
    return tax;
}

/// Returns the total social security tax owed based on the income breakdown.
/// Applies to wages only, capped at the annual wage base.
USD social_security(const Inflation &inflation, const Day &date, const Taxes::Entry &entry) {
    // Social Security wage base — approximate for 2026, verify annually
    static const auto kSSWageBase = 180000_USD;
    const USD wage_base = inflation(date, kSSWageBase);

    return std::min(entry.income.standard, wage_base) * 0.062;
}

} // namespace

CalculatedTaxes calculate_taxes(const double avg_inflation, const Taxes &taxes, const I64 year) {
    // Assume tax constants are adjusted for inflation based on the end of that tax year.
    // All original constants in this file are from 2026, so use that as the reference date for future inflation.
    const Inflation inflation (Day::end_of_year(2026), avg_inflation);
    const Day date = Day::end_of_year(year);

    // Standard deduction for MFJ in 2026
    static const auto kStandardDeduction = 32000_USD;
    const auto standard_deduction = inflation(date, kStandardDeduction);

    CalculatedTaxes result;
    const Taxes::Entry entry = taxes.get(Day::end_of_year(year));

    // Standard deduction (MFJ). Deductions like donations only help if itemizing; use whichever is larger.
    // Deductions are limited to 60% of gross income for most itemized deductions.
    USD deduction = std::max(standard_deduction, entry.deductions.total());
    deduction = std::min(deduction, entry.income.total() * 0.6);

    // Ordinary income = wages + short-term capital gains, reduced by deduction.
    const USD gross_ordinary = entry.income.standard + entry.income.short_term_gains;
    const USD ordinary_taxable = std::max(0.00_USD, gross_ordinary - deduction);

    // Qualified dividends and LTCG use preferential rates, stacked on top of ordinary taxable.
    const USD preferential = entry.income.dividends + entry.income.long_term_gains;

    result.year            = year;
    result.federal_income  = income_tax(inflation, date, ordinary_taxable);
    result.ltcg            = long_term_capital_gains_tax(inflation, date, ordinary_taxable, preferential);
    result.niit            = niit_tax(inflation, date, entry);
    result.medicare        = medicare(inflation, date, entry);
    result.social_security = social_security(inflation, date, entry);
    result.entry           = entry;
    return result;
}
