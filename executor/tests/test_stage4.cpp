/**
 * @file test_stage4.cpp
 * @brief Финальные тесты подзадач 6 и 7: DML/SELECT, WHERE, индекс, JSON.
 */
#include "executor.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace {

bool contains(const std::string& text, const std::string& token) {
    return text.find(token) != std::string::npos;
}

std::filesystem::path make_temp_root(const std::string& suffix) {
    const auto root = std::filesystem::temp_directory_path() / ("executor_stage4_" + suffix);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

}  // namespace

TEST(ExecutorStage4, InsertUpdateDeleteAndSelectJson) {
    const auto root = make_temp_root("crud");
    executor::DbmsQueryEngine engine(root);

    EXPECT_EQ(engine.execute("CREATE DATABASE app;"), "OK");
    EXPECT_EQ(engine.execute("USE app;"), "OK");
    EXPECT_EQ(engine.execute("CREATE TABLE users (id int INDEXED, name string NOT_NULL, city string);"), "OK");

    EXPECT_EQ(engine.execute("INSERT INTO users (id, name, city) VALUE (1, \"Ann\", \"Minsk\"), (2, \"Bob\", \"Brest\");"), "OK");
    EXPECT_EQ(engine.execute("UPDATE users SET city = \"Grodno\" WHERE id == 2;"), "OK");

    const std::string selected = engine.execute("SELECT id AS user_id, city FROM users WHERE id == 2;");
    EXPECT_TRUE(contains(selected, "\"user_id\":2"));
    EXPECT_TRUE(contains(selected, "\"city\":\"Grodno\""));

    EXPECT_EQ(engine.execute("DELETE FROM users WHERE name == \"Ann\";"), "OK");
    const std::string afterDelete = engine.execute("SELECT * FROM users;");
    EXPECT_FALSE(contains(afterDelete, "\"name\":\"Ann\""));
    EXPECT_TRUE(contains(afterDelete, "\"name\":\"Bob\""));
}

TEST(ExecutorStage4, ConditionEvaluatorBetweenLikeAndComparisons) {
    const auto root = make_temp_root("conditions");
    executor::DbmsQueryEngine engine(root);

    EXPECT_EQ(engine.execute("CREATE DATABASE app;"), "OK");
    EXPECT_EQ(engine.execute("USE app;"), "OK");
    EXPECT_EQ(engine.execute("CREATE TABLE logs (id int INDEXED, msg string NOT_NULL);"), "OK");
    EXPECT_EQ(engine.execute("INSERT INTO logs (id, msg) VALUE (10, \"alpha\"), (11, \"beta\"), (12, \"alphabet\");"), "OK");

    const std::string between = engine.execute("SELECT * FROM logs WHERE id BETWEEN 10 AND 12;");
    EXPECT_TRUE(contains(between, "\"id\":10"));
    EXPECT_TRUE(contains(between, "\"id\":11"));
    EXPECT_FALSE(contains(between, "\"id\":12"));  // [10,12)

    const std::string like = engine.execute("SELECT * FROM logs WHERE msg LIKE \"alpha.*\";");
    EXPECT_TRUE(contains(like, "\"msg\":\"alpha\""));
    EXPECT_TRUE(contains(like, "\"msg\":\"alphabet\""));
    EXPECT_FALSE(contains(like, "\"msg\":\"beta\""));

    const std::string cmp = engine.execute("SELECT * FROM logs WHERE id >= 11;");
    EXPECT_FALSE(contains(cmp, "\"id\":10"));
    EXPECT_TRUE(contains(cmp, "\"id\":11"));
}

TEST(ExecutorStage4, OptimizerUsesIndexWhenPossible) {
    const auto root = make_temp_root("index");
    executor::DbmsQueryEngine engine(root);

    EXPECT_EQ(engine.execute("CREATE DATABASE app;"), "OK");
    EXPECT_EQ(engine.execute("USE app;"), "OK");
    EXPECT_EQ(engine.execute("CREATE TABLE t (id int INDEXED, payload string);"), "OK");
    EXPECT_EQ(engine.execute("INSERT INTO t (id, payload) VALUE (1, \"x\"), (2, \"y\"), (3, \"z\");"), "OK");

    (void)engine.execute("SELECT * FROM t WHERE id == 2;");
    EXPECT_TRUE(engine.lastSelectUsedIndex());

    (void)engine.execute("SELECT * FROM t WHERE payload == \"y\";");
    EXPECT_FALSE(engine.lastSelectUsedIndex());
}

TEST(ExecutorStage4, SupportsDbDotTableAndUseContext) {
    const auto root = make_temp_root("dbref");
    executor::DbmsQueryEngine engine(root);

    EXPECT_EQ(engine.execute("CREATE DATABASE d1;"), "OK");
    EXPECT_EQ(engine.execute("CREATE TABLE d1.t (id int INDEXED, name string);"), "OK");
    EXPECT_EQ(engine.execute("INSERT INTO d1.t (id, name) VALUE (1, \"N\");"), "OK");

    const std::string viaDbDot = engine.execute("SELECT * FROM d1.t WHERE id == 1;");
    EXPECT_TRUE(contains(viaDbDot, "\"name\":\"N\""));

    EXPECT_EQ(engine.execute("USE d1;"), "OK");
    const std::string viaUse = engine.execute("SELECT * FROM t WHERE id == 1;");
    EXPECT_TRUE(contains(viaUse, "\"name\":\"N\""));
}

