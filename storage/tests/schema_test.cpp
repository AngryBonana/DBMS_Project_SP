#include <gtest/gtest.h>

#include "sql/schema.h"

using namespace cw_db;

TEST(TableSchemaTest, BuildAndLookupColumns) {
    TableSchema schema({
        {"id", DataType::Int, true, true, std::nullopt},
        {"name", DataType::Str, false, false, std::nullopt},
        {"age", DataType::Int, false, false, Value::of_int(18)}
    });

    EXPECT_EQ(schema.column_count(), 3u);
    EXPECT_FALSE(schema.empty());
    EXPECT_TRUE(schema.has_column("id"));
    EXPECT_EQ(schema.index_of("name"), 1u);

    const auto& age = schema.column("age");
    EXPECT_EQ(age.name, "age");
    EXPECT_EQ(age.type, DataType::Int);
    EXPECT_TRUE(age.default_value.has_value());
    EXPECT_TRUE(age.default_value->is_int());
    EXPECT_EQ(age.default_value->as_int(), 18);
}

TEST(TableSchemaTest, RejectsDuplicateOrEmptyNames) {
    EXPECT_THROW((TableSchema({
        {"id", DataType::Int, false, false, std::nullopt},
        {"id", DataType::Str, false, false, std::nullopt}
    })), std::invalid_argument);

    EXPECT_THROW((TableSchema({
        {"", DataType::Int, false, false, std::nullopt}
    })), std::invalid_argument);
}

TEST(TableSchemaTest, MissingColumnThrows) {
    TableSchema schema({
        {"id", DataType::Int, true, false, std::nullopt}
    });

    EXPECT_FALSE(schema.has_column("missing"));
    EXPECT_FALSE(schema.find_index("missing").has_value());
    EXPECT_THROW(schema.index_of("missing"), std::out_of_range);
    EXPECT_THROW(schema.column("missing"), std::out_of_range);
}