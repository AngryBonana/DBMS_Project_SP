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

}  // namespace

TEST(ExecutorStage1, GeneratesGuidV4) {
    const auto id = executor::generateRequestId();
    EXPECT_TRUE(executor::isValidRequestId(id));
    EXPECT_NE(executor::generateRequestId(), id);
}

TEST(ExecutorStage1, SubmitReturnsIdAndCompletesAsync) {
    executor::AsyncExecutor exec([](const std::string& query) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        return std::string("echo:") + query;
    });

    const auto id = exec.submit("SELECT 1;", "client-1");
    EXPECT_TRUE(executor::isValidRequestId(id));

    const auto pending = exec.getSnapshot(id);
    ASSERT_TRUE(pending.has_value());
    EXPECT_EQ(pending->id, id);

    ASSERT_TRUE(exec.waitUntilFinished(id, std::chrono::seconds(2)));

    const auto done = exec.getSnapshot(id);
    ASSERT_TRUE(done.has_value());
    EXPECT_EQ(done->status, executor::RequestStatus::Completed);
    ASSERT_TRUE(done->result.has_value());
    EXPECT_EQ(*done->result, "echo:SELECT 1;");
}

TEST(ExecutorStage1, UnknownRequestReturnsEmptySnapshot) {
    executor::AsyncExecutor exec([](const std::string& query) {
        return query;
    });

    EXPECT_FALSE(exec.getSnapshot("00000000-0000-4000-8000-000000000000")
                     .has_value());
}

TEST(ExecutorStage1, AccessLoggerWritesRequestFields) {
    const std::string logPath = "executor_stage1_access.log";

    {
        executor::AccessLogger logger(logPath);
        executor::AsyncExecutor exec(
            [](const std::string&) { return std::string("OK"); }, &logger);

        const auto id = exec.submit("CREATE DATABASE test;", "client-42");
        ASSERT_TRUE(exec.waitUntilFinished(id, std::chrono::seconds(2)));
    }

    const std::string content = readFile(logPath);
    EXPECT_NE(content.find("client_id=client-42"), std::string::npos);
    EXPECT_NE(content.find("handler_id="), std::string::npos);
    EXPECT_NE(content.find("body=\"CREATE DATABASE test;\""),
              std::string::npos);
    EXPECT_NE(content.find("status_code=0"), std::string::npos);
    EXPECT_NE(content.find("start="), std::string::npos);
    EXPECT_NE(content.find("end="), std::string::npos);

    std::remove(logPath.c_str());
}
