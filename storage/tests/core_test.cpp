#include <gtest/gtest.h>

#include "core/string_pool.h"
#include "core/value.h"
#include "sql/schema.h"

using namespace cw_db;

TEST(ValueTest, CompareAndEquality) {
    const Value i10a = Value::of_int(10);
    const Value i10b = Value::of_int(10);
    const Value i20 = Value::of_int(20);
    const Value hello = Value::of_str("hello");
    const Value world = Value::of_str("world");
    const Value n = Value::null();

    EXPECT_TRUE(i10a == i10b);
    EXPECT_FALSE(i10a == i20);
    EXPECT_TRUE(hello == Value::of_str("hello"));
    EXPECT_FALSE(hello == world);

    bool valid = false;
    EXPECT_EQ(i10a.compare(i10b, valid), 0);
    EXPECT_TRUE(valid);
    EXPECT_LT(i10a.compare(i20, valid), 0);
    EXPECT_EQ(hello.compare(Value::of_str("hello"), valid), 0);

    EXPECT_TRUE(n.is_null());
    EXPECT_FALSE(i10a.matches(DataType::Str));
    EXPECT_FALSE(n == i10a);
}

TEST(StringPoolTest, InternGetAndInvalidId) {
    auto& pool = StringPool::instance();
    const auto id1 = pool.intern("abc");
    const auto id2 = pool.intern(std::string("abc"));
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(pool.get(id1), "abc");
    EXPECT_THROW(pool.get(StringPool::kInvalid), std::runtime_error);
}

TEST(TableSchemaTest, BuildLookupAndRejectInvalid) {
    TableSchema schema({
        {"id", DataType::Int, true, true, std::nullopt},
        {"name", DataType::Str, false, false, std::nullopt},
        {"age", DataType::Int, false, false, Value::of_int(18)},
    });

    EXPECT_EQ(schema.column_count(), 3u);
    EXPECT_FALSE(schema.empty());
    EXPECT_TRUE(schema.has_column("id"));
    EXPECT_EQ(schema.index_of("name"), 1u);
    EXPECT_EQ(schema.column("age").default_value->as_int(), 18);

    EXPECT_THROW((TableSchema({
        {"id", DataType::Int, false, false, std::nullopt},
        {"id", DataType::Str, false, false, std::nullopt},
    })), std::invalid_argument);
    EXPECT_THROW((TableSchema({
        {"", DataType::Int, false, false, std::nullopt},
    })), std::invalid_argument);

    EXPECT_FALSE(schema.has_column("missing"));
    EXPECT_THROW(schema.index_of("missing"), std::out_of_range);
    EXPECT_THROW(schema.column("missing"), std::out_of_range);
}
