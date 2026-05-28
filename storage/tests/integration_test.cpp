#include <fstream>
#include <gtest/gtest.h>

#include "core/dbms.h"
#include "storage/database.h"
#include "test_helpers.hpp"

using namespace cw_db;
using namespace cw_db::test;

namespace {

void seed_users_and_logs(Database& db) {
    Table& users = db.create_table("users", users_schema());
    Table& logs = db.create_table("logs", id_name_schema());
    users.insert_row({Value::of_int(1), Value::of_str("Bob")});
    users.insert_row({Value::of_int(2), std::nullopt});
    logs.insert_row({Value::of_int(7), Value::of_str("hello")});
}

void expect_users_and_logs(Database& db) {
    ASSERT_TRUE(db.has_table("users"));
    ASSERT_TRUE(db.has_table("logs"));

    const Table& users = db.require_table("users");
    const Table& logs = db.require_table("logs");
    EXPECT_EQ(users.row_count(), 2u);
    EXPECT_EQ(logs.row_count(), 1u);
    EXPECT_EQ(users.get_row(0)[0].as_int(), 1);
    EXPECT_EQ(users.get_row(0)[1].as_str(), "Bob");
    EXPECT_EQ(users.get_row(1)[0].as_int(), 2);
    EXPECT_EQ(users.get_row(1)[1].as_str(), "anon");
    EXPECT_EQ(logs.get_row(0)[0].as_int(), 7);
    EXPECT_EQ(logs.get_row(0)[1].as_str(), "hello");
}

} // namespace

TEST(PersistenceTest, DatabaseSaveLoadRoundtrip) {
    TempDir dir("storage_db_roundtrip");
    Database db("testdb");
    seed_users_and_logs(db);
    db.save_to(dir.path());

    Database loaded("testdb");
    loaded.load_from(dir.path());
    expect_users_and_logs(loaded);
}

TEST(PersistenceTest, DbmsSaveLoadRoundtrip) {
    TempDir root("storage_dbms_roundtrip");
    Dbms dbms(root.string());
    Database& db = dbms.create_database("main");
    dbms.use_database("main");
    seed_users_and_logs(db);
    dbms.save_all();

    Dbms restored(root.string());
    restored.load_all();
    ASSERT_TRUE(restored.has_database("main"));
    expect_users_and_logs(restored.require_database("main"));
}

TEST(PersistenceTest, DatabaseLoadRejectsCorruptTableFile) {
    TempDir dir("storage_db_corrupt");
    {
        std::ofstream out(dir.path() / "broken.dat", std::ios::binary);
        const char magic[4] = {'C', 'W', 'D', 'B'};
        write_table_header(out, magic, 2);
        const std::uint32_t pool_count = 1;
        out.write(reinterpret_cast<const char*>(&pool_count), sizeof(pool_count));
    }

    Database db("testdb");
    EXPECT_THROW(db.load_from(dir.path()), std::runtime_error);
}
