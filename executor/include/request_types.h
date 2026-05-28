/**
 * @file request_types.h
 * @brief Типы данных и вспомогательные функции для работы с запросами.
 *
 * Определяет идентификатор запроса (RequestId), перечисление статусов,
 * структуру снимка состояния RequestSnapshot, структуру записи лога доступа,
 * а также функции генерации и валидации идентификаторов по стандарту UUIDv4.
 */
#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace executor {

using RequestId = std::string;

enum class RequestStatus {
    Pending,
    Running,
    Completed,
    Failed
};

inline const char* toString(RequestStatus status) {
    switch (status) {
    case RequestStatus::Pending:   return "pending";
    case RequestStatus::Running:   return "running";
    case RequestStatus::Completed: return "completed";
    case RequestStatus::Failed:    return "failed";
    }
    return "unknown";
}

struct RequestSnapshot {
    RequestId id;
    RequestStatus status = RequestStatus::Pending;
    std::optional<std::string> result;
    std::optional<std::string> error;
};

struct AccessLogEntry {
    std::string requestBody;
    std::string clientId;
    std::string handlerId;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    int statusCode = 0;
    std::string statusMessage;
};

RequestId generateRequestId();

bool isValidRequestId(const std::string& id);

}  // namespace executor