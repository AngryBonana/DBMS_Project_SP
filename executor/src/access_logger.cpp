/**
 * @file access_logger.cpp
 * @brief Реализация логгера доступа.
 *
 * Выполняет открытие файла лога (с проверкой), форматирование времени
 * в локальную строку и потокобезопасную запись записей с использованием
 * mutex'а. Каждая запись содержит временные метки, клиента, обработчик,
 * код и текст статуса, а также тело запроса.
 */
#include "access_logger.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace executor {

AccessLogger::AccessLogger(std::string logFilePath)
    : logFilePath_(std::move(logFilePath)),
      file_(logFilePath_, std::ios::app) {
    if (!file_.is_open()) {
        throw std::runtime_error("Can't open access log file: " + logFilePath_);
    }
}

void AccessLogger::logRequest(const AccessLogEntry& entry) {
    const auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                entry.endTime - entry.startTime)
                                .count();

    std::lock_guard<std::mutex> lock(mutex_);
    file_ << "[ACCESS]"
          << " client_id=" << entry.clientId
          << " handler_id=" << entry.handlerId
          << " start=" << formatTime(entry.startTime)
          << " end=" << formatTime(entry.endTime)
          << " duration_ms=" << durationMs
          << " status_code=" << entry.statusCode
          << " status_msg=\"" << entry.statusMessage << "\""
          << " body=\"" << entry.requestBody << "\""
          << '\n';
    file_.flush();
}

std::string AccessLogger::formatTime(
    const std::chrono::system_clock::time_point& tp) const {
    const std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

}  // namespace executor