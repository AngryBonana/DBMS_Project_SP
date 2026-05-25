#include <gtest/gtest.h>
#include "sql/schema.h"
#include "storage/table.h"
#include <filesystem>

using namespace cw_db;

TEST(TablePersistTest, SaveLoadRoundtrip) {
    TableSchema schema({
        {"id", DataType::Int, true, false, std::nullopt},
        {"name", DataType::Str, false, false, std::optional<Value>(Value::of_str("anon"))},
        {"age", DataType::Int, false, false, std::optional<Value>(Value::of_int(18))}
    });

    Table t(schema);
    t.insert_row({Value::of_int(1), Value::of_str("Bob"), std::nullopt});
    t.insert_row({Value::of_int(2), std::nullopt, Value::of_int(30)});

    auto tmp = std::filesystem::temp_directory_path() / "table_test.dat";
    t.save(tmp.string());

    Table t2 = Table::load(tmp.string());
    EXPECT_EQ(t2.row_count(), 2u);
    EXPECT_EQ(t2.get_row(0).size(), 3u);
    EXPECT_EQ(t2.get_row(0)[0].as_int(), 1);
    EXPECT_EQ(t2.get_row(0)[1].as_str(), "Bob");
    EXPECT_EQ(t2.get_row(0)[2].as_int(), 18);

    EXPECT_EQ(t2.get_row(1)[0].as_int(), 2);
    EXPECT_EQ(t2.get_row(1)[1].as_str(), "anon");
    EXPECT_EQ(t2.get_row(1)[2].as_int(), 30);
}
