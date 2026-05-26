#include <gtest/gtest.h>
#include "sql/schema.h"
#include "storage/table.h"

using namespace cw_db;

TEST(TableTest, InsertAndDefaults) {
    TableSchema schema({
        {"id", DataType::Int, true, false, std::nullopt},
        {"name", DataType::Str, false, false, std::optional<Value>(Value::of_str("anon"))},
        {"age", DataType::Int, false, false, std::optional<Value>(Value::of_int(18))}
    });

    Table t(schema);

    // Вставляем только id и name - age должен подставиться
    t.insert_row({Value::of_int(1), Value::of_str("Bob")});
    EXPECT_EQ(t.row_count(), 1u);
    const auto& r = t.get_row(0);
    EXPECT_TRUE(r[0].is_int());
    EXPECT_EQ(r[0].as_int(), 1);
    EXPECT_TRUE(r[1].is_str());
    EXPECT_EQ(r[1].as_str(), "Bob");
    EXPECT_TRUE(r[2].is_int());
    EXPECT_EQ(r[2].as_int(), 18);
}

TEST(TableTest, MissingNotNullThrows) {
    TableSchema schema({
        {"id", DataType::Int, true, false, std::nullopt},
        {"name", DataType::Str, false, false, std::nullopt}
    });
    Table t(schema);
    // id NOT NULL missing
    EXPECT_THROW(t.insert_row({std::nullopt, Value::of_str("X")}), std::invalid_argument);
}

TEST(TableTest, TypeMismatchThrows) {
    TableSchema schema({
        {"id", DataType::Int, true, false, std::nullopt},
        {"name", DataType::Str, false, false, std::nullopt}
    });
    Table t(schema);
    // type mismatch: string into int
    EXPECT_THROW(t.insert_row({Value::of_str("bad"), Value::of_str("X")}), std::invalid_argument);
}

TEST(TableTest, RowIdsStayStableAfterErase) {
    TableSchema schema({
        {"id", DataType::Int, true, true, std::nullopt},
        {"name", DataType::Str, false, false, std::nullopt}
    });

    Table table(schema);
    const RowId first = table.insert(Row{{Value::of_int(1), Value::of_str("A")}});
    const RowId second = table.insert(Row{{Value::of_int(2), Value::of_str("B")}});

    EXPECT_NE(first, second);
    EXPECT_EQ(table.row_count(), 2u);

    table.erase(first);
    EXPECT_EQ(table.row_count(), 1u);
    EXPECT_EQ(table.get_row(0)[0].as_int(), 2);
    EXPECT_EQ(table.get_row(0)[1].as_str(), "B");
}
