#include <gtest/gtest.h>

#include "canni/data/Day.h"

using namespace canni;

namespace {

Day jan1_2024() { return Day("01/01/2024"); }
Day feb1_2024() { return Day("02/01/2024"); }
Day jan1_2025() { return Day("01/01/2025"); }

} // namespace

TEST(Day, ParseValid) {
    const auto day = Day::parse("06/15/2024");
    ASSERT_TRUE(day.has_value());
    EXPECT_EQ(day->month(), Month::Jun);  // tm_mon is 0-indexed
    EXPECT_EQ(day->day(), 15);
    EXPECT_EQ(day->year(), 2024);
}

TEST(Day, ParseInvalid) {
    EXPECT_FALSE(Day::parse("not-a-date").has_value());
    EXPECT_FALSE(Day::parse("").has_value());
}

TEST(Day, ParseMMMDDYYYY) {
    const auto day = Day::parse("Jan 15 2024", "%b %d %Y");
    ASSERT_TRUE(day.has_value());
    EXPECT_EQ(day->month(), Month::Jan);
    EXPECT_EQ(day->day(), 15);
    EXPECT_EQ(day->year(), 2024);

    EXPECT_EQ(Day::parse("Dec 31 2025", "%b %d %Y")->month(), Month::Dec);
    EXPECT_FALSE(Day::parse("Xyz 01 2024", "%b %d %Y").has_value());
    EXPECT_FALSE(Day::parse("Jan 01 2024",  "%b %d, %Y").has_value()); // missing comma
}

TEST(Day, Comparison) {
    EXPECT_LT(jan1_2024(), feb1_2024());
    EXPECT_GT(feb1_2024(), jan1_2024());
    EXPECT_EQ(jan1_2024(), jan1_2024());
    EXPECT_NE(jan1_2024(), feb1_2024());
}

TEST(Day, DifferenceInDays) {
    // February 2024 is a leap year, so Jan->Feb is 31 days
    EXPECT_EQ(feb1_2024() - jan1_2024(), 31);
    EXPECT_EQ(jan1_2024() - feb1_2024(), -31);
    EXPECT_EQ(jan1_2024() - jan1_2024(), 0);
}

TEST(Day, DifferenceAcrossYear) {
    EXPECT_EQ(jan1_2025() - jan1_2024(), 366); // 2024 is a leap year
    EXPECT_EQ(Day("1/1/2026") - Day("1/1/2025"), 365); // 2025 is not a leap year
}

TEST(Day, DifferenceAcrossLeapDay) {
    // Feb 28 -> Mar 1 is 1 day in non-leap year, 2 days in leap year
    EXPECT_EQ(Day("3/1/2025") - Day("2/28/2025"), 1);
    EXPECT_EQ(Day("3/1/2024") - Day("2/28/2024"), 2); // Feb has 29 days in 2024
}

TEST(Day, DifferenceMultiYear) {
    // 2024 + 2025 = 366 + 365 = 731 days
    EXPECT_EQ(Day("1/1/2026") - Day("1/1/2024"), 731);
}

TEST(Day, NextMonth) {
    const auto jan = jan1_2024();
    const auto feb = jan.next_month();
    EXPECT_EQ(feb, feb1_2024());

    const auto dec = *Day::parse("12/01/2024");
    const auto jan_next = dec.next_month();
    EXPECT_EQ(jan_next, jan1_2025());
}

TEST(Day, AddDistance) {
    EXPECT_EQ(Day("1/1/2024") + 1_years, Day("1/1/2025"));
    EXPECT_EQ(Day("1/1/2024") + 11_months, Day("12/1/2024"));
    EXPECT_EQ(Day("1/1/2024") + 15_months, Day("4/1/2025"));
}

TEST(Day, ToString) {
    EXPECT_EQ(jan1_2024().to_string(), "01/01/2024");
}

TEST(Day, MonthFields) {
    const auto day = Day("03/15/2024");
    EXPECT_EQ(day.day(), 15);
    EXPECT_EQ(day.month(), Month::Mar);
    EXPECT_EQ(day.year(), 2024);
}
