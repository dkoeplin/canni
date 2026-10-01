#include <gtest/gtest.h>

#include "canni/data/USD.h"

using namespace canni;

TEST(USD, Arithmetic) {
    EXPECT_EQ(USD::dollars(10) + USD::dollars(5), USD::dollars(15));
    EXPECT_EQ(USD::dollars(10) - USD::dollars(3), USD::dollars(7));
    EXPECT_EQ(USD::dollars(10) * 3, USD::dollars(30));
    EXPECT_EQ(-USD::dollars(5), USD::dollars(-5));
}

TEST(USD, Comparison) {
    EXPECT_LT(USD::dollars(1), USD::dollars(2));
    EXPECT_GT(USD::dollars(2), USD::dollars(1));
    EXPECT_EQ(USD::dollars(5), USD::dollars(5));
    EXPECT_NE(USD::dollars(5), USD::dollars(6));
}

TEST(USD, Round) {
    EXPECT_EQ(USD::floor(1.005), USD::dollars(1));  // rounds to nearest cent
    EXPECT_EQ(USD::floor(1.456), USD::floor(1.46));
    EXPECT_EQ(USD::floor(-1.50), USD::floor(-1.50)); // -150 cents
    EXPECT_DOUBLE_EQ(USD::floor(-1.50).f64(), -1.50);
}

TEST(USD, F64) {
    EXPECT_DOUBLE_EQ(USD::dollars(10).f64(), 10.0);
    EXPECT_DOUBLE_EQ(USD::floor(3.14).f64(), 3.14);
}

TEST(USD, ParseSimple) {
    EXPECT_EQ(USD::parse("100"), USD::dollars(100));
    EXPECT_EQ(USD::parse("3.14"), USD::floor(3.14));
    EXPECT_EQ(USD::parse("0.99"), USD::floor(0.99));
}

TEST(USD, ParseNegative) {
    EXPECT_EQ(USD::parse("-50.25"), USD::floor(-50.25));
    EXPECT_EQ(USD::parse("-100"), USD::dollars(-100));
}

TEST(USD, ParseEmpty) {
    EXPECT_EQ(USD::parse(""), USD::dollars(0));
}

TEST(USD, ParseDollarSign) {
    EXPECT_EQ(USD::parse("$1234.56"), USD::floor(1234.56));
    EXPECT_EQ(USD::parse("$-50.00"), USD::floor(-50.0));
}

TEST(USD, ToString) {
    EXPECT_EQ(USD::dollars(0).to_string(), "$0.00");
    EXPECT_EQ(USD::dollars(1).to_string(), "$1.00");
    EXPECT_EQ(USD::floor(3.14).to_string(), "$3.14");
    EXPECT_EQ(USD::floor(3.05).to_string(), "$3.05");
    EXPECT_EQ(USD::dollars(-5).to_string(), "-$5.00");
    EXPECT_EQ(USD::floor(-1.50).to_string(), "-$1.50");
    EXPECT_EQ(USD::floor(-3.07).to_string(), "-$3.07");
}

TEST(USD, UDL) {
    EXPECT_EQ(100_USD, USD::dollars(100));
    EXPECT_EQ(3.14_USD, USD::floor(3.14));
}

TEST(USD, CompoundAssign) {
    USD a = USD::dollars(10);
    a += USD::dollars(5);
    EXPECT_EQ(a, USD::dollars(15));
    a -= USD::dollars(3);
    EXPECT_EQ(a, USD::dollars(12));
}
