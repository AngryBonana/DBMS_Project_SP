/**
 * @file executor_service.cpp
 * @brief Реализация фасада ExecutorService.
 */
#include "executor_service.h"

#include "json_response.h"
#include "query_classifier.h"
#include "request_api.h"

namespace executor {

ExecutorService::ExecutorService(QueryHandler handler, ExecutorConfig config)
    : config_(std::move(config)),
      handler_(std::move(handler)),
      logger_(std::make_unique<AccessLogger>(config_.accessLogPath)),
      executor_(std::make_unique<AsyncExecutor>(
          handler_, logger_.get(), config_.maxStoredRequests)) {}

std::string ExecutorService::submit(const std::string& query,
                                    const std::string& clientId) {
    // DDL/DML — в очередь с request_id; SELECT — сразу в потоке клиента.
    if (classifyExecutionMode(query) == ExecutionMode::Async) {
        const RequestId id = executor_->submit(query, clientId);
        return buildAsyncAcceptedResponse(id);
    }
    return executeSync(query, clientId);
}

std::string ExecutorService::getStatusJson(const std::string& requestId) const {
    if (!isValidRequestId(requestId)) {
        return buildErrorResponse("Invalid request_id format",
                                  static_cast<int>(ReturnCode::Error));
    }

    const auto status = executor_->getStatus(requestId);
    if (!status) {
        return buildErrorResponse("Request not found",
                                  static_cast<int>(ReturnCode::NotFound));
    }

    return buildStatusResponse(*status);
}

std::string ExecutorService::getResultJson(const std::string& requestId) const {
    if (!isValidRequestId(requestId)) {
        return buildErrorResponse("Invalid request_id format",
                                  static_cast<int>(ReturnCode::Error));
    }

    const auto result = executor_->getResult(requestId);
    if (!result) {
        return buildErrorResponse("Request not found",
                                  static_cast<int>(ReturnCode::NotFound));
    }

    return buildResultResponse(*result);
}

std::string ExecutorService::handleClientCommand(const std::string& command,
                                                 const std::string& clientId) {
    const ApiCommand parsed = parseApiCommand(command);
    switch (parsed.kind) {
    case ApiCommandKind::GetStatus:
        return getStatusJson(parsed.payload);
    case ApiCommandKind::GetResult:
        return getResultJson(parsed.payload);
    case ApiCommandKind::SubmitQuery:
        return submit(parsed.payload, clientId);
    }
    return buildErrorResponse("Unsupported command",
                              static_cast<int>(ReturnCode::Error));
}

std::string ExecutorService::executeSync(const std::string& query,
                                         const std::string& clientId) {
    const auto startTime = std::chrono::system_clock::now();
    const std::string handlerId = "sync-" + generateRequestId();

    int statusCode = static_cast<int>(ReturnCode::Ok);
    std::string statusMessage = "OK";
    std::string responseBody;

    try {
        responseBody = handler_(query);
    } catch (const std::exception& ex) {
        statusCode = static_cast<int>(ReturnCode::Error);
        statusMessage = ex.what();
        responseBody = std::string("ERROR: ") + statusMessage;
    }

    const auto endTime = std::chrono::system_clock::now();
    logger_->logRequest(AccessLogEntry{
        query,
        clientId,
        handlerId,
        startTime,
        endTime,
        statusCode,
        statusMessage,
    });

    return buildSyncResponse(responseBody, statusCode);
}

}  // namespace executor
