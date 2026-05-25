#include <gtest/gtest.h>
#include "core/dbms.h"
#include "sql/schema.h"
#include "core/value.h"
#include <filesystem>

using namespace cw_db;

TEST(DbmsIntegrationTest, SaveLoadRoundtrip) {
    auto root = std::filesystem::temp_directory_path() / "dbms_test_root";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    Dbms dbms(root);
    Database& db = dbms.create_database("main");
    dbms.use_database("main");

    TableSchema users_schema({
        {"id", DataType::Int, true, false, std::nullopt},
        {"name", DataType::Str, false, false, std::optional<Value>(Value::of_str("anon"))}
    });

    TableSchema logs_schema({
        {"k", DataType::Int, true, false, std::nullopt},
        {"msg", DataType::Str, false, false, std::nullopt}
    });

    Table& users = db.create_table("users", users_schema);
    Table& logs = db.create_table("logs", logs_schema);

    users.insert_row({Value::of_int(1), Value::of_str("Bob")});
    users.insert_row({Value::of_int(2), std::nullopt});
    logs.insert_row({Value::of_int(7), Value::of_str("hello")});

    dbms.save_all();

    Dbms loaded(root);
    loaded.load_all();

    ASSERT_TRUE(loaded.has_database("main"));
    Database& loaded_db = loaded.require_database("main");
    ASSERT_TRUE(loaded_db.has_table("users"));
    ASSERT_TRUE(loaded_db.has_table("logs"));

    Table& loaded_users = loaded_db.require_table("users");
    Table& loaded_logs = loaded_db.require_table("logs");

    EXPECT_EQ(loaded_users.row_count(), 2u);
    EXPECT_EQ(loaded_logs.row_count(), 1u);

    EXPECT_EQ(loaded_users.get_row(0)[0].as_int(), 1);
    EXPECT_EQ(loaded_users.get_row(0)[1].as_str(), "Bob");
    EXPECT_EQ(loaded_users.get_row(1)[0].as_int(), 2);
    EXPECT_EQ(loaded_users.get_row(1)[1].as_str(), "anon");
    EXPECT_EQ(loaded_logs.get_row(0)[0].as_int(), 7);
    EXPECT_EQ(loaded_logs.get_row(0)[1].as_str(), "hello");

    std::filesystem::remove_all(root);
}
