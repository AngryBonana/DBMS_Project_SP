#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <string>
#include <chrono>
#include <mutex>
#include <ctime>

class Logger {
public:
    explicit Logger(const std::string& path);

    struct RequestLog {
        std::string clientId;
        std::string handlerId;
        std::string requestBody;
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point endTime;
        int statusCode;
        std::string statusMsg;
    };

    void logConnect(const std::string& clientId);
    void logDisconnect(const std::string& clientId);
    void logRequest(const RequestLog& entry);

private:
    std::string formatTime(const std::chrono::system_clock::time_point& tp) const;

    std::ofstream file_;
    std::mutex mutex_;
};

#endif // LOGGER_H