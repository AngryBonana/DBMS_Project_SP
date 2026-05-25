#include <gtest/gtest.h>
#include "storage/database.h"
#include "sql/schema.h"
#include "core/value.h"
#include <filesystem>

using namespace cw_db;

TEST(DatabaseTest, SaveLoadRoundtrip) {
    // Временная папка для теста
    auto tmpdir = std::filesystem::temp_directory_path() / "db_test_dir";
    std::filesystem::remove_all(tmpdir);
    std::filesystem::create_directories(tmpdir);

    Database db("testdb");

    TableSchema s1({
        {"id", DataType::Int, true, false, std::nullopt},
        {"name", DataType::Str, false, false, std::optional<Value>(Value::of_str("anon"))}
    });

    TableSchema s2({
        {"k", DataType::Int, true, false, std::nullopt},
        {"v", DataType::Str, false, false, std::nullopt}
    });

    // Создадим таблицы
    Table& t1 = db.create_table("users", s1);
    Table& t2 = db.create_table("pairs", s2);

    t1.insert_row({Value::of_int(1), Value::of_str("Bob")});
    t1.insert_row({Value::of_int(2), std::nullopt});

    t2.insert_row({Value::of_int(10), Value::of_str("X")});

    db.save_to(tmpdir);

    // Загружаем в новую Database
    Database db2("testdb");
    db2.load_from(tmpdir);

    ASSERT_TRUE(db2.has_table("users"));
    ASSERT_TRUE(db2.has_table("pairs"));

    Table& r1 = db2.require_table("users");
    Table& r2 = db2.require_table("pairs");

    EXPECT_EQ(r1.row_count(), 2u);
    EXPECT_EQ(r2.row_count(), 1u);

    EXPECT_EQ(r1.get_row(0)[0].as_int(), 1);
    EXPECT_EQ(r1.get_row(0)[1].as_str(), "Bob");
    EXPECT_EQ(r1.get_row(1)[0].as_int(), 2);
    EXPECT_EQ(r1.get_row(1)[1].as_str(), "anon");

    // cleanup
    std::filesystem::remove_all(tmpdir);
}
