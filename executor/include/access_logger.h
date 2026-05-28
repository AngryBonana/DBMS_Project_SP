/**
 * @file access_logger.h
 * @brief Логгер доступа для асинхронного исполнителя.
 *
 * Предоставляет класс AccessLogger, который записывает в файл структурированные
 * записи о каждом обработанном запросе: клиент, обработчик, время выполнения,
 * статус и тело запроса. Потокобезопасен.
 */
#pragma once

#include "request_types.h"

#include <fstream>
#include <mutex>
#include <string>

namespace executor {

class AccessLogger {
public:
    explicit AccessLogger(std::string logFilePath);

    void logRequest(const AccessLogEntry& entry);

private:
    std::string formatTime(const std::chrono::system_clock::time_point& tp) const;

    std::string logFilePath_;
    std::ofstream file_;
    mutable std::mutex mutex_;
};

}  // namespace executor