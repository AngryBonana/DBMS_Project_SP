/**
 * @file query_classifier.cpp
 * @brief Реализация эвристической классификации запросов.
 */
#include "query_classifier.h"
#include "string_utils.h"

#include <cctype>
#include <string>

namespace executor {

namespace {

/// Возвращает первое «слово» запроса (до пробела или скобки).
std::string firstToken(const std::string& upperQuery) {
    std::string token;
    for (const char ch : upperQuery) {
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            token.push_back(ch);
        } else {
            break;
        }
    }
    return token;
}

}  // namespace

ExecutionMode classifyExecutionMode(const std::string& query) {
    const std::string normalized = toUpperAscii(trim(query));
    if (normalized.empty()) {
        return ExecutionMode::Sync;
    }

    const std::string keyword = firstToken(normalized);

    // DDL и массовые DML — потенциально длительные (ТЗ: «длительная операция»).
    if (keyword == "CREATE" || keyword == "DROP" || keyword == "INSERT" ||
        keyword == "UPDATE" || keyword == "DELETE") {
        return ExecutionMode::Async;
    }

    // SELECT и прочее (USE и т.п.) — синхронно на этом этапе.
    return ExecutionMode::Sync;
}

}  // namespace executor
