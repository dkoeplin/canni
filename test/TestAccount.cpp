#include <gtest/gtest.h>

#include "Accounts.h"
#include "Day.h"
#include "Interest.h"
#include "USD.h"
#include "nvl/data/Tensor.h"

namespace {

// Build a minimal 2D string tensor from rows of {date, balance} pairs.
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

TEST(Account, ParseBasic) {
    const auto data = make_tensor({
        {"01/01/2024", "1000.00"},
        {"02/01/2024", "1100.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);
    EXPECT_EQ(account.name(), "Test");

    const auto dates = account.dates();
    ASSERT_EQ(dates.size(), 2);
    EXPECT_EQ(*account.get(Day("01/01/2024")), USD::round(1000.00));
    EXPECT_EQ(*account.get(Day("02/01/2024")), USD::round(1100.00));
}

TEST(Account, ParseSkipsInvalidDates) {
    const auto data = make_tensor({
        {"not-a-date", "999.00"},
        {"01/01/2024", "1000.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);
    EXPECT_EQ(account.dates().size(), 1);
    EXPECT_TRUE(account.get(Day("01/01/2024")).has_value());
}

TEST(Account, ParseWithPrincipal) {
    const auto data = make_tensor({
        {"01/01/2024", "1200.00", "1000.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1, /*principal*/2);
    EXPECT_EQ(*account.get(Day("01/01/2024")), USD::round(1200.00));
}

TEST(Account, GetReturnsNoneForMissingDate) {
    const auto data = make_tensor({
        {"01/01/2024", "1000.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);
    EXPECT_FALSE(account.get(Day("06/15/2024")).has_value());
}

TEST(Account, ProjectionLinearInterpolation) {
    // With two known points, projection between them should be linear.
    const auto data = make_tensor({
        {"01/01/2024", "1000.00"},
        {"03/01/2024", "1060.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);

    // Feb 1 is between Jan 1 and Mar 1 (31 days in, 60 days total span)
    // slope = 60/59 days ~ $1.017/day, so after 31 days ~ $1031.53
    const USD projected = account[Day("02/01/2024")];
    EXPECT_GT(projected, USD::dollars(1025));
    EXPECT_LT(projected, USD::dollars(1040));
}

TEST(Account, ProjectionInterestExtrapolation) {
    // Beyond last known point, uses interest model.
    const auto data = make_tensor({
        {"01/01/2024", "1000.00"},
    });
    const auto account = Cash(data, "Test", Interest::Monthly(0.12), /*balance*/1);

    // One year out should be close to 1000 * 1.12 = $1120
    const USD projected = account[Day("01/01/2025")];
    EXPECT_GT(projected, USD::dollars(1115));
    EXPECT_LT(projected, USD::dollars(1125));
}

TEST(Account, ProjectionMemoized) {
    const auto data = make_tensor({
        {"01/01/2024", "1000.00"},
        {"03/01/2024", "1060.00"},
    });
    const auto account = Cash(data, "Test", Interest::Simple(0.0), /*balance*/1);

    const USD first  = account[Day("02/01/2024")];
    const USD second = account[Day("02/01/2024")];
    EXPECT_EQ(first, second);
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
