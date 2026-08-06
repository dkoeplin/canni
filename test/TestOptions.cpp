#include "gtest/gtest.h"

#include "invest/account/Options.h"

namespace {

TEST(TestOptions, OptionExpiration) {
    const Day exp ("1/1/2035");
    Company company ("Name");
    company.valuation(Day("1/1/2025"), 1.00_USD, 5.00_USD);
    Options options(company, "Grant", Options::kNSO, 1.00_USD);
    options.vesting(Day("1/1/2025"), {.nso = 50});
    options.sell(Day("1/1/2025"), {.nso = 25});
    options.set_exp(exp);

    Day day ("1/1/2025");
    const Day end ("1/1/2040");
    while (day < end) {
        ASSERT_EQ(options[day], day < exp ? 100.00_USD : 0.00_USD) << "Mismatch on " << day;
        day = day.next_month();
    }
}

} // namespace