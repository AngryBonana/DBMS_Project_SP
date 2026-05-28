/**
 * @file request_api.h
 * @brief Парсинг служебных API-команд executor для клиентского протокола.
 *
 * На этапе 3 вводим компактный текстовый протокол:
 *   - GET STATUS <guid>;
 *   - GET RESULT <guid>;
 * Остальные строки трактуются как SQL-запросы и идут в submit().
 */
#pragma once

#include <optional>
#include <string>

namespace executor {

enum class ApiCommandKind {
    SubmitQuery,
    GetStatus,
    GetResult
};

struct ApiCommand {
    ApiCommandKind kind = ApiCommandKind::SubmitQuery;
    std::string payload;
};

/// Пытается распознать служебную команду; при неуспехе возвращает SubmitQuery.
ApiCommand parseApiCommand(const std::string& rawCommand);

}  // namespace executor
