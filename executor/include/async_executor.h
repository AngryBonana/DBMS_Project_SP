/**
 * @file async_executor.h
 * @brief Асинхронный исполнитель запросов с очередью задач.
 *
 * Содержит класс AsyncExecutor, который принимает пользовательские запросы,
 * назначает им уникальный идентификатор, обрабатывает их в фоновом потоке
 * и предоставляет доступ к снимкам состояния (статус, результат, ошибка).
 * Поддерживает ожидание завершения конкретного запроса и опциональное
 * логирование через AccessLogger.
 */
#pragma once

#include "access_logger.h"
#include "request_types.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

namespace executor {

using QueryHandler = std::function<std::string(const std::string&)>;

class AsyncExecutor {
public:
    AsyncExecutor(QueryHandler handler, AccessLogger* accessLogger = nullptr);
    ~AsyncExecutor();

    AsyncExecutor(const AsyncExecutor&) = delete;
    AsyncExecutor& operator=(const AsyncExecutor&) = delete;

    RequestId submit(const std::string& query, const std::string& clientId);

    std::optional<RequestSnapshot> getSnapshot(const RequestId& id) const;

    bool waitUntilFinished(const RequestId& id,
                           std::chrono::milliseconds timeout);

private:
    struct Job {
        RequestId id;
        std::string clientId;
        std::string query;
    };

    void workerLoop();
    void processJob(const Job& job);
    void updateSnapshot(const RequestId& id, RequestStatus status,
                        const std::optional<std::string>& result,
                        const std::optional<std::string>& error);

    QueryHandler handler_;
    AccessLogger* accessLogger_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable finishedCv_;
    std::queue<Job> queue_;
    std::unordered_map<RequestId, RequestSnapshot> snapshots_;
    bool stop_ = false;

    std::thread worker_;
};

}  // namespace executor