#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

#include "storage/table.h"
#include "test_helpers.hpp"

using namespace cw_db;
using namespace cw_db::test;

namespace {

class MockIndex : public IIndex {
public:
    MOCK_METHOD(void, insert, (const Value& key, RowId row_id), (override));
    MOCK_METHOD(void, erase, (const Value& key, RowId row_id), (override));
    MOCK_METHOD(std::vector<RowId>, find, (const Value& key), (const, override));
    MOCK_METHOD(void, clear, (), (override));
};

} // namespace

TEST(TableTest, InsertAppliesDefaults) {
    Table t(users_schema());
    t.insert_row({Value::of_int(1), Value::of_str("Bob")});

    ASSERT_EQ(t.row_count(), 1u);
    const auto& row = t.get_row(0);
    EXPECT_EQ(row[0].as_int(), 1);
    EXPECT_EQ(row[1].as_str(), "Bob");
    EXPECT_EQ(row[2].as_int(), 18);
}

TEST(TableTest, RejectsInvalidRows) {
    Table t(id_name_schema(true));
    EXPECT_THROW(t.insert_row({std::nullopt, Value::of_str("X")}), std::invalid_argument);
    EXPECT_THROW(t.insert_row({Value::of_str("bad"), Value::of_str("X")}), std::invalid_argument);
    EXPECT_THROW(t.insert_row({Value::of_int(1), Value::of_int(2)}), std::invalid_argument);
}

TEST(TableTest, SaveLoadRoundtrip) {
    Table t(users_schema());
    t.insert_row({Value::of_int(1), Value::of_str("Bob"), std::nullopt});
    t.insert_row({Value::of_int(2), std::nullopt, Value::of_int(30)});

    const auto path = std::filesystem::temp_directory_path() / "storage_table_roundtrip.dat";
    t.save(path.string());

    const Table loaded = Table::load(path.string());
    EXPECT_EQ(loaded.row_count(), 2u);
    EXPECT_EQ(loaded.get_row(0)[0].as_int(), 1);
    EXPECT_EQ(loaded.get_row(0)[1].as_str(), "Bob");
    EXPECT_EQ(loaded.get_row(0)[2].as_int(), 18);
    EXPECT_EQ(loaded.get_row(1)[0].as_int(), 2);
    EXPECT_EQ(loaded.get_row(1)[1].as_str(), "anon");
    EXPECT_EQ(loaded.get_row(1)[2].as_int(), 30);

    std::filesystem::remove(path);
}

TEST(TableTest, LoadRejectsCorruptFiles) {
    const auto bad_magic = std::filesystem::temp_directory_path() / "storage_bad_magic.dat";
    {
        std::ofstream out(bad_magic, std::ios::binary);
        const char magic[4] = {'B', 'A', 'D', '!'};
        write_table_header(out, magic, 2);
    }
    EXPECT_THROW((void)Table::load(bad_magic.string()), std::runtime_error);
    std::filesystem::remove(bad_magic);

    const auto bad_version = std::filesystem::temp_directory_path() / "storage_bad_version.dat";
    {
        std::ofstream out(bad_version, std::ios::binary);
        const char magic[4] = {'C', 'W', 'D', 'B'};
        write_table_header(out, magic, 0);
    }
    EXPECT_THROW((void)Table::load(bad_version.string()), std::runtime_error);
    std::filesystem::remove(bad_version);
}

TEST(TableTest, IndexHooksRebuildAndTrackInserts) {
    Table table(indexed_id_schema());
    table.insert_row({Value::of_int(1), Value::of_str("one")});

    auto index = std::make_shared<MockIndex>();
    EXPECT_CALL(*index, insert(::testing::Truly([](const Value& v) {
        return v.is_int() && v.as_int() == 1;
    }), 0));
    table.attach_index(0, index, true);

    EXPECT_CALL(*index, insert(::testing::Truly([](const Value& v) {
        return v.is_int() && v.as_int() == 2;
    }), 1));
    table.insert_row({Value::of_int(2), Value::of_str("two")});

    EXPECT_TRUE(table.has_index(0));
    table.detach_index(0);
    EXPECT_FALSE(table.has_index(0));
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
