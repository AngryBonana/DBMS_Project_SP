/**
 * @file query_classifier.cpp
 * @brief Реализация эвристической классификации запросов.
 */
#include "query_classifier.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace executor {

namespace {

/// Убирает пробелы и переводы строк в начале и конце строки.
std::string trim(const std::string& text) {
    const auto notSpace = [](unsigned char ch) {
        return !std::isspace(ch);
    };

    const auto begin =
        std::find_if(text.begin(), text.end(), notSpace);
    const auto end =
        std::find_if(text.rbegin(), text.rend(), notSpace).base();

    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

/// Приводит ASCII-символы к верхнему регистру (для сравнения ключевых слов).
std::string toUpperAscii(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return text;
}

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
        keyword == "UPDATE" || keyword == "DELETE" || keyword == "REVERT") {
        return ExecutionMode::Async;
    }

    // SELECT и прочее (USE и т.п.) — синхронно на этом этапе.
    return ExecutionMode::Sync;
}

}  // namespace executor
