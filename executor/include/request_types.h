/**
 * @file request_types.h
 * @brief Типы данных и вспомогательные функции для работы с запросами.
 *
 * Определяет идентификатор запроса (RequestId, UUID v4), жизненный цикл
 * (RequestStatus), снимки для внутреннего хранения и структуры публичного API
 * (RequestStatusInfo, RequestResultInfo), а также запись журнала доступа.
 */
#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace executor {

using RequestId = std::string;

/// Состояние запроса в очереди асинхронного исполнителя.
enum class RequestStatus {
    Pending,    ///< Принят, ожидает рабочий поток
    Running,    ///< Выполняется обработчиком
    Completed,  ///< Успешно завершён, результат доступен
    Failed      ///< Завершён с ошибкой
};

/// Код возврата для журнала и JSON-ответов (0 — успех).
enum class ReturnCode {
    Ok = 0,
    Error = 1,
    NotFound = 404,
    NotReady = 202
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

/// Полный внутренний снимок запроса (хранится в AsyncExecutor).
struct RequestSnapshot {
    RequestId id;
    RequestStatus status = RequestStatus::Pending;
    std::optional<std::string> result;
    std::optional<std::string> error;
    std::chrono::system_clock::time_point submittedAt{};
    std::optional<std::chrono::system_clock::time_point> startedAt;
    std::optional<std::chrono::system_clock::time_point> finishedAt;
};

/// Публичная информация о статусе (без тела результата) — API getStatus.
struct RequestStatusInfo {
    RequestId id;
    RequestStatus status = RequestStatus::Pending;
    std::chrono::system_clock::time_point submittedAt{};
    std::optional<std::chrono::system_clock::time_point> startedAt;
    std::optional<std::chrono::system_clock::time_point> finishedAt;
};

/// Публичная информация о результате — API getResult.
struct RequestResultInfo {
    bool ready = false;
    RequestId id;
    RequestStatus status = RequestStatus::Pending;
    std::optional<std::string> result;
    std::optional<std::string> error;
};

/// Запись журнала доступа (задание 7): все поля из ТЗ.
struct AccessLogEntry {
    std::string requestBody;
    std::string clientId;
    std::string handlerId;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    int statusCode = 0;
    std::string statusMessage;
};

/// Генерация идентификатора запроса в формате UUID version 4.
RequestId generateRequestId();

/// Проверка строки на соответствие формату GUID v4.
bool isValidRequestId(const std::string& id);

/// ISO-подобная метка времени для JSON (локальное время сервера).
std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);

}  // namespace executor
