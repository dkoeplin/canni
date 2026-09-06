#include <gtest/gtest.h>

#include "../invest/account/Accounts.h"
#include "Day.h"
#include "Interest.h"
#include "USD.h"
#include "nvl/data/Tensor.h"

namespace {

nvl::Tensor<2, std::string> make_tensor(const std::vector<std::vector<std::string>> &rows) {
    const I64 nrows = static_cast<I64>(rows.size());
    const I64 ncols = rows.empty() ? 0 : static_cast<I64>(rows[0].size());
    nvl::Tensor<2, std::string> t(nvl::Pos<2>(nrows, ncols), "");
    for (I64 r = 0; r < nrows; ++r)
        for (I64 c = 0; c < ncols; ++c)
            t[nvl::Pos<2>(r, c)] = rows[r][c];
    return t;
}

} // namespace

TEST(Account, ParseSetsNameAndBalance) {
    const auto data = make_tensor({
        {"01/01/2024", "1000.00"},
        {"02/01/2024", "1100.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);
    EXPECT_EQ(account.name(), "Test");
    // balance_ is initialized from the last row
    EXPECT_EQ(account.balance(), USD::round(1100.00));
    EXPECT_EQ(account.principal(), USD::round(1100.00));
}

TEST(Account, ParseSkipsInvalidDates) {
    const auto data = make_tensor({
        {"not-a-date", "999.00"},
        {"01/01/2024", "1000.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);
    EXPECT_EQ(account.dates().size(), 1);
}

TEST(Account, ParseWithPrincipal) {
    const auto data = make_tensor({
        {"01/01/2024", "1200.00", "1000.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1, /*principal*/2);
    EXPECT_EQ(account.balance(), USD::round(1200.00));
    EXPECT_EQ(account.principal(), USD::round(1000.00));
}

TEST(Account, HistoricalGetLookup) {
    const auto data = make_tensor({
        {"01/01/2024", "1000.00"},
        {"03/01/2024", "1060.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);
    // get() does a history lookup (most recent at or before date)
    const auto jan = account.get(Day("01/01/2024"));
    EXPECT_EQ(jan.balance, USD::round(1000.00));
    const auto feb = account.get(Day("02/01/2024")); // between entries → returns Jan
    EXPECT_EQ(feb.balance, USD::round(1000.00));
    const auto mar = account.get(Day("03/01/2024"));
    EXPECT_EQ(mar.balance, USD::round(1060.00));
}

TEST(Account, GetBeforeFirstEntryReturnsEmpty) {
    const auto data = make_tensor({
        {"03/01/2024", "1000.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);
    const auto e = account.get(Day("01/01/2024")); // before any entry
    EXPECT_EQ(e.balance, 0_USD);
    EXPECT_EQ(e.principal, 0_USD);
}

TEST(Account, AdvanceAppliesInterest) {
    const auto data = make_tensor({
        {"01/01/2024", "1000.00"},
    });
    auto account = Cash(data, "Test", Interest::Monthly(0.12), /*balance*/1);
    Taxes taxes;
    account.advance(taxes, Day("01/01/2024"), Day("01/01/2025"));
    // 12% yearly ≈ $1120 after one year
    EXPECT_GT(account.balance(), USD::dollars(1115));
    EXPECT_LT(account.balance(), USD::dollars(1125));
}

TEST(Account, CloneIsIndependent) {
    const auto data = make_tensor({{"01/01/2024", "1000.00"}});
    auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);
    auto cloned = account.clone();
    Taxes taxes;
    account.deposit(Day("01/01/2024"), 500_USD);
    // Clone should not be affected
    EXPECT_EQ(account.balance(), USD::dollars(1500));
    EXPECT_EQ(cloned->balance(), USD::dollars(1000));
}

TEST(Account, Dates) {
    const auto data = make_tensor({
        {"01/01/2024", "1000.00"},
        {"02/01/2024", "1050.00"},
        {"03/01/2024", "1100.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);
    const auto dates = account.dates();
    ASSERT_EQ(dates.size(), 3);
    EXPECT_EQ(dates[0], Day("01/01/2024"));
    EXPECT_EQ(dates[1], Day("02/01/2024"));
    EXPECT_EQ(dates[2], Day("03/01/2024"));
}
