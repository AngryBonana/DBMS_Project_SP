#include "../include/logger.h"
#include <iomanip>
#include <sstream>

Logger::Logger(const std::string& path)
    : file_(path, std::ios::app)
{
    if (!file_.is_open())
        throw std::runtime_error("Can't open log file: " + path);
}

void Logger::logConnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    file_ << "[CONNECT] client_id=" << clientId
        << " time=" << formatTime(std::chrono::system_clock::now())
        << "\n";
    file_.flush();
}

void Logger::logDisconnect(const std::string& clientId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    file_ << "[DISCONNECT] client_id=" << clientId
        << " time=" << formatTime(std::chrono::system_clock::now())
        << "\n";
    file_.flush();
}

void Logger::logRequest(const RequestLog& entry)
{
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.endTime - entry.startTime
    ).count();

    std::lock_guard<std::mutex> lock(mutex_);
    file_ << "[REQUEST]"
        << " client_id=" << entry.clientId
        << " handler_id=" << entry.handlerId
        << " start=" << formatTime(entry.startTime)
        << " end=" << formatTime(entry.endTime)
        << " duration_ms=" << duration
        << " status=" << (entry.statusCode == 0 ? "OK" : "ERROR")
        << " status_msg=\"" << entry.statusMsg << "\""
        << " body=\"" << entry.requestBody << "\""
        << "\n";
    file_.flush();
}

std::string Logger::formatTime(const std::chrono::system_clock::time_point& tp) const
{
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
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