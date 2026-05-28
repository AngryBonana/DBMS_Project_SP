/**
 * @file json_response.h
 * @brief Формирование JSON-ответов API исполнителя.
 *
 * Сервер сможет передавать клиенту готовые JSON-строки без дублирования
 * логики сериализации. Все строки экранируются для безопасной вставки в JSON.
 */
#pragma once

#include "request_types.h"

#include <string>

namespace executor {

/// Экранирует спецсимволы для использования внутри JSON-строки.
std::string escapeJsonString(const std::string& value);

/// Ответ на синхронно выполненный запрос.
std::string buildSyncResponse(const std::string& result, int statusCode);

/// Ответ при принятии асинхронного запроса (возврат GUID).
std::string buildAsyncAcceptedResponse(const RequestId& requestId);

/// Ответ метода получения статуса.
std::string buildStatusResponse(const RequestStatusInfo& info);

/// Ответ метода получения результата (готов или ещё выполняется).
std::string buildResultResponse(const RequestResultInfo& info);

/// Ответ об ошибке API (неверный GUID, неизвестный запрос и т.д.).
std::string buildErrorResponse(const std::string& message, int code);

}  // namespace executor
