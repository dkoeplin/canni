#include "Taxes.h"

#include "canni/data/Inflation.h"

namespace canni {

namespace {

/// Assumes tax constants are adjusted for inflation based on the end of that tax year.
/// All original constants in this file are from 2026, so use that as the reference date for future inflation.
const Day kReference (1, Month::Jan, 2026);

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
        const USD min = i == 0 ? 0.00_USD : inflation(kReference, date, brackets[i - 1].max);
        const USD max = inflation(kReference, date, bracket.max);
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
        const USD max = inflation(kReference, date, max_income);
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
    static constexpr auto kSSWageBase = 180000_USD;
    const USD wage_base = inflation(kReference, date, kSSWageBase);
    return std::min(entry.income.standard, wage_base) * 0.062;
}

/// IRS Uniform Lifetime Table III divisors, starting at age 73.
double rmd_factor(const I64 age) {
    static constexpr std::array<double, 20> kFactors = {
        26.5, 25.5, 24.6, 23.7, 22.9, 22.0, 21.1, 20.2, 19.4, 18.5,
        17.7, 16.8, 16.0, 15.2, 14.4, 13.7, 12.9, 12.2, 11.5, 10.8
    };
    const I64 idx = age - 73;
    if (idx < 0) return 0.0;
    return idx < static_cast<I64>(kFactors.size()) ? kFactors[idx] : 5.0;
}

} // namespace

USD Taxes::yearly_401k_contribution_limit(const I64 year) {
    const auto years = year - 2026;
    return 24500_USD + (500_USD * years);
}

double Taxes::required_minimum_distribution(const I64 age) {
    return rmd_factor(age);
}

const Taxes::Entry &Taxes::get(const Day& day) const {
    static constexpr Entry kEmpty = {};
    const I64 idx = day.year() - base_year_;
    return (!entries_.empty() && idx >= 0 && idx < static_cast<I64>(entries_.size()))
        ? entries_[idx] : kEmpty;
}

CalculatedTaxes calculate_taxes(const Inflation &inflation, const Taxes &taxes, const I64 year) {
    const Day date = Day::end_of_year(year);

    // Standard deduction for MFJ in 2026
    static constexpr auto kStandardDeduction = 32000_USD;
    const auto standard_deduction = inflation(kReference, date, kStandardDeduction);

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

} // namespace canni
