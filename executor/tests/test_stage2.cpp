/**
 * @file test_stage2.cpp
 * @brief Тесты этапа 2: API статуса/результата, классификатор, JSON-фасад.
 */
#include "executor.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST(ExecutorStage2, ClassifierMarksDdlAsAsync) {
    EXPECT_EQ(executor::classifyExecutionMode("CREATE DATABASE db1;"),
              executor::ExecutionMode::Async);
    EXPECT_EQ(executor::classifyExecutionMode("  insert into t values (1);"),
              executor::ExecutionMode::Async);
    EXPECT_EQ(executor::classifyExecutionMode("SELECT * FROM t;"),
              executor::ExecutionMode::Sync);
}

TEST(ExecutorStage2, SubmitAsyncReturnsRequestIdJson) {
    executor::ExecutorConfig config;
    config.accessLogPath = "executor_stage2_async.log";

    executor::ExecutorService service(
        [](const std::string&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            return std::string("DONE");
        },
        config);

    const std::string response =
        service.submit("CREATE TABLE t (id int);", "client-a");

    EXPECT_TRUE(contains(response, "\"mode\":\"async\""));
    EXPECT_TRUE(contains(response, "\"request_id\":"));

    std::remove(config.accessLogPath.c_str());
}

TEST(ExecutorStage2, SubmitSyncSelectReturnsBareJsonArray) {
    executor::ExecutorService service(
        [](const std::string&) { return std::string(R"([{"id":1,"name":"Ann"}])"); });

    const std::string response =
        service.submit("SELECT * FROM users;", "client-select");

    EXPECT_EQ(response, R"([{"id":1,"name":"Ann"}])");
    EXPECT_FALSE(contains(response, "\"mode\":\"sync\""));
}

TEST(ExecutorStage2, SubmitSyncReturnsImmediateResult) {
    executor::ExecutorConfig config;
    config.accessLogPath = "executor_stage2_sync.log";

    executor::ExecutorService service(
        [](const std::string& query) { return std::string("R:") + query; },
        config);

    const std::string response =
        service.submit("SELECT 1 FROM t;", "client-b");

    EXPECT_TRUE(contains(response, "\"mode\":\"sync\""));
    EXPECT_TRUE(contains(response, "R:SELECT 1 FROM t;"));

    const std::string log = readFile(config.accessLogPath);
    EXPECT_TRUE(contains(log, "client_id=client-b"));
    EXPECT_TRUE(contains(log, "handler_id=sync-"));

    std::remove(config.accessLogPath.c_str());
}

TEST(ExecutorStage2, StatusAndResultApiLifecycle) {
    executor::ExecutorConfig config;
    config.accessLogPath = "executor_stage2_lifecycle.log";

    executor::ExecutorService service(
        [](const std::string&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return std::string("[{\"id\":1}]");
        },
        config);

    const std::string accepted =
        service.submit("DELETE FROM t WHERE id == 1;", "client-c");
    ASSERT_TRUE(contains(accepted, "\"request_id\":"));

    const auto pos = accepted.find("\"request_id\":\"");
    ASSERT_NE(pos, std::string::npos);
    const auto idStart = pos + std::string("\"request_id\":\"").size();
    const auto idEnd = accepted.find('"', idStart);
    const std::string requestId = accepted.substr(idStart, idEnd - idStart);

    ASSERT_TRUE(executor::isValidRequestId(requestId));

    const std::string statusPending = service.getStatusJson(requestId);
    EXPECT_TRUE(contains(statusPending, "\"status\":\"pending\"") ||
                contains(statusPending, "\"status\":\"running\""));

    ASSERT_TRUE(service.executor().waitUntilFinished(
        requestId, std::chrono::seconds(2)));

    const std::string statusDone = service.getStatusJson(requestId);
    EXPECT_TRUE(contains(statusDone, "\"status\":\"completed\""));

    const std::string resultReady = service.getResultJson(requestId);
    EXPECT_TRUE(contains(resultReady, "\"ready\":true"));
    EXPECT_TRUE(contains(resultReady, "[{\"id\":1}]"));

    const std::string log = readFile(config.accessLogPath);
    EXPECT_TRUE(contains(log, "handler_id=handler-"));

    std::remove(config.accessLogPath.c_str());
}

TEST(ExecutorStage2, ResultNotReadyWhilePending) {
    executor::ExecutorService service([](const std::string&) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return std::string("late");
    });

    const std::string accepted =
        service.submit("UPDATE t SET x = 1;", "client-d");
    const auto pos = accepted.find("\"request_id\":\"");
    const auto idStart = pos + std::string("\"request_id\":\"").size();
    const auto idEnd = accepted.find('"', idStart);
    const std::string requestId = accepted.substr(idStart, idEnd - idStart);

    const std::string resultJson = service.getResultJson(requestId);
    EXPECT_TRUE(contains(resultJson, "\"ready\":false"));
}

TEST(ExecutorStage2, UnknownRequestReturnsNotFound) {
    executor::ExecutorService service([](const std::string&) { return "x"; });

    const std::string status = service.getStatusJson(
        "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");
    EXPECT_TRUE(contains(status, "\"code\":404"));

    const std::string badFormat = service.getStatusJson("not-a-guid");
    EXPECT_TRUE(contains(badFormat, "Invalid request_id"));
}
