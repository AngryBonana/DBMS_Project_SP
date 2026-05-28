/**
 * @file test_sql_smoke.cpp
 * @brief Smoke-тест SQL-сценария из demo/sql через DbmsQueryEngine.
 */
#include "executor.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace {

bool contains(const std::string& text, const std::string& token) {
    return text.find(token) != std::string::npos;
}

std::filesystem::path make_temp_root() {
    const auto root =
        std::filesystem::temp_directory_path() / "executor_sql_smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void run_setup(executor::DbmsQueryEngine& engine) {
    EXPECT_EQ(engine.execute("CREATE DATABASE demo;"), "OK");
    EXPECT_EQ(engine.execute("USE demo;"), "OK");
    EXPECT_EQ(engine.execute(
                  "CREATE TABLE users (id INT NOT_NULL INDEXED, name STRING NOT_NULL, city STRING);"),
              "OK");
    EXPECT_EQ(engine.execute(
                  "CREATE TABLE logs (id INT NOT_NULL INDEXED, msg STRING NOT_NULL);"),
              "OK");
    EXPECT_EQ(engine.execute(
                  "INSERT INTO users (id, name, city) VALUES (1, 'Ann', 'Minsk'), (2, 'Bob', 'Brest');"),
              "OK");
    EXPECT_EQ(engine.execute(
                  "INSERT INTO logs (id, msg) VALUES (10, 'alpha'), (11, 'beta'), (12, 'alphabet');"),
              "OK");
}

} // namespace

TEST(SqlSmokeTest, DemoSetupAndQueries) {
    const auto root = make_temp_root();
    executor::DbmsQueryEngine engine(root);
    run_setup(engine);

    const std::string selected =
        engine.execute("SELECT id, name, city FROM users WHERE id == 2;");
    EXPECT_TRUE(contains(selected, "\"name\":\"Bob\""));
    EXPECT_TRUE(contains(selected, "\"city\":\"Brest\""));

    EXPECT_EQ(engine.execute("UPDATE users SET city = 'Grodno' WHERE id == 2;"), "OK");

    const std::string updated = engine.execute("SELECT * FROM users WHERE id == 2;");
    EXPECT_TRUE(contains(updated, "\"city\":\"Grodno\""));

    const std::string between = engine.execute("SELECT * FROM logs WHERE id BETWEEN 10 AND 12;");
    EXPECT_TRUE(contains(between, "\"id\":10"));
    EXPECT_TRUE(contains(between, "\"id\":11"));
}

TEST(SqlSmokeTest, DataPersistsAfterReload) {
    const auto root = make_temp_root();
    {
        executor::DbmsQueryEngine engine(root);
        EXPECT_EQ(engine.execute("CREATE DATABASE demo;"), "OK");
        EXPECT_EQ(engine.execute("USE demo;"), "OK");
        EXPECT_EQ(engine.execute(
                      "CREATE TABLE notes (id INT NOT_NULL, title STRING NOT_NULL);"),
                  "OK");
        EXPECT_EQ(engine.execute("INSERT INTO notes (id, title) VALUES (1, 'persist');"), "OK");
    }

    executor::DbmsQueryEngine reloaded(root);
    EXPECT_EQ(reloaded.execute("USE demo;"), "OK");

    const std::string rows = reloaded.execute("SELECT * FROM notes;");
    EXPECT_TRUE(contains(rows, "\"title\":\"persist\""));
}
