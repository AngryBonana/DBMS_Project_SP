#include <gtest/gtest.h>
#include "core/value.h"

using namespace cw_db;

TEST(ValueTest, IntEqualityAndCompare) {
    Value a = Value::of_int(10);
    Value b = Value::of_int(10);
    Value c = Value::of_int(20);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);

    bool valid = false;
    EXPECT_EQ(a.compare(b, valid), 0);
    EXPECT_TRUE(valid);
    EXPECT_LT(a.compare(c, valid), 0);
}

TEST(ValueTest, StringEqualityAndCompare) {
    Value a = Value::of_str("hello");
    Value b = Value::of_str("hello");
    Value c = Value::of_str("world");

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);

    bool valid = false;
    EXPECT_EQ(a.compare(b, valid), 0);
    EXPECT_TRUE(valid);
    EXPECT_LT(a.compare(c, valid), 0);
}

TEST(ValueTest, NullAndTypeMismatch) {
    Value n = Value::null();
    Value i = Value::of_int(1);

    bool valid = true;
    EXPECT_EQ(n.is_null(), true);
    EXPECT_FALSE(n.matches(DataType::Int) == false);

    EXPECT_FALSE(i.matches(DataType::Str));
    EXPECT_FALSE(n == i);
    EXPECT_FALSE(i == Value::of_str("1"));
}
