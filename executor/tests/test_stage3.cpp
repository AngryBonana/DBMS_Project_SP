/**
 * @file test_stage3.cpp
 * @brief Тесты этапа 3: текстовый API и детализация статуса очереди.
 */
#include "executor.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::string extractRequestId(const std::string& acceptedJson) {
    const auto pos = acceptedJson.find("\"request_id\":\"");
    if (pos == std::string::npos) {
        return {};
    }
    const auto start = pos + std::string("\"request_id\":\"").size();
    const auto end = acceptedJson.find('"', start);
    if (end == std::string::npos) {
        return {};
    }
    return acceptedJson.substr(start, end - start);
}

}  // namespace

TEST(ExecutorStage3, ParseApiCommandRecognizesStatusAndResult) {
    const auto statusCmd =
        executor::parseApiCommand("GET STATUS aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee;");
    EXPECT_EQ(statusCmd.kind, executor::ApiCommandKind::GetStatus);
    EXPECT_EQ(statusCmd.payload, "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

    const auto resultCmd =
        executor::parseApiCommand("get result 12345678-1234-4abc-8def-123456789abc");
    EXPECT_EQ(resultCmd.kind, executor::ApiCommandKind::GetResult);
    EXPECT_EQ(resultCmd.payload, "12345678-1234-4abc-8def-123456789abc");

    const auto queryCmd = executor::parseApiCommand("SELECT * FROM t;");
    EXPECT_EQ(queryCmd.kind, executor::ApiCommandKind::SubmitQuery);
}

TEST(ExecutorStage3, HandleClientCommandRoutesServiceApi) {
    executor::ExecutorService service([](const std::string&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        return std::string("ok");
    });

    const std::string accepted =
        service.handleClientCommand("INSERT INTO t (id) VALUE (1);", "cli-1");
    const std::string requestId = extractRequestId(accepted);
    ASSERT_TRUE(executor::isValidRequestId(requestId));

    const std::string statusJson = service.handleClientCommand(
        "GET STATUS " + requestId + ";", "cli-1");
    EXPECT_TRUE(contains(statusJson, "\"status\":\"pending\"") ||
                contains(statusJson, "\"status\":\"running\""));

    ASSERT_TRUE(service.executor().waitUntilFinished(
        requestId, std::chrono::seconds(2)));

    const std::string resultJson = service.handleClientCommand(
        "GET RESULT " + requestId + ";", "cli-1");
    EXPECT_TRUE(contains(resultJson, "\"ready\":true"));
}

TEST(ExecutorStage3, PendingStatusContainsQueuePosition) {
    executor::ExecutorService service([](const std::string&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        return std::string("ok");
    });

    const std::string accepted1 =
        service.submit("DELETE FROM t WHERE id == 1;", "c1");
    const std::string accepted2 =
        service.submit("DELETE FROM t WHERE id == 2;", "c2");

    const std::string id1 = extractRequestId(accepted1);
    const std::string id2 = extractRequestId(accepted2);
    ASSERT_FALSE(id1.empty());
    ASSERT_FALSE(id2.empty());

    // Второй запрос с высокой вероятностью ещё стоит в очереди.
    const std::string status2 = service.getStatusJson(id2);
    EXPECT_TRUE(contains(status2, "\"queue_position\":") ||
                contains(status2, "\"status\":\"running\""));
}

