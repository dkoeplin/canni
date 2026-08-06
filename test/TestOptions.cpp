#include "gtest/gtest.h"

#include "invest/account/Options.h"

namespace {

TEST(TestOptions, OptionExpiration) {
    const Day exp ("1/1/2035");
    Company company ("Name");
    company.valuation(Day("1/1/2025"), 1.00_USD, 5.00_USD);
    Options options(company, "Grant", Options::kNSO, 1.00_USD);
    options.vesting(Day("1/1/2025"), {.nso = 50});

    Taxes taxes;
    (void)options.sell(taxes, Day("1/1/2025"), 5.00_USD, {.nso = 25}, false);
    options.set_exp(exp);

    // After the sell, advance each month and check the balance.
    // Vested=50, sold=25 => 25 remaining; gain = (5.00 - 1.00) * 25 = 100.00
    Day prev("1/1/2025");
    Day day("1/1/2025");
    const Day end("1/1/2040");
    while (day < end) {
        options.advance(taxes, prev, day);
        const USD expected = day < exp ? 100.00_USD : 0.00_USD;
        ASSERT_EQ(options.balance(), expected) << "Mismatch on " << day;
        prev = day;
        day = day.next_month();
    }
}

} // namespace
