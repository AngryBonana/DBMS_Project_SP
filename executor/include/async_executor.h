/**
 * @file async_executor.h
 * @brief Асинхронный исполнитель запросов с очередью задач.
 *
 * Принимает запросы через submit, обрабатывает их в фоновом потоке
 * и предоставляет раздельное API статуса и результата (задание 6).
 * Опционально пишет access-log через AccessLogger (задание 7).
 */
#pragma once

#include "access_logger.h"
#include "request_types.h"

#include <atomic>
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
    AsyncExecutor(QueryHandler handler, AccessLogger* accessLogger = nullptr,
                  std::size_t maxStoredRequests = 4096);
    ~AsyncExecutor();

    AsyncExecutor(const AsyncExecutor&) = delete;
    AsyncExecutor& operator=(const AsyncExecutor&) = delete;

    /// Поставить запрос в очередь; вернуть GUID немедленно.
    RequestId submit(const std::string& query, const std::string& clientId);

    /// Полный снимок (для тестов и отладки).
    std::optional<RequestSnapshot> getSnapshot(const RequestId& id) const;

    /// Только статус и метки времени — API «получить статус».
    std::optional<RequestStatusInfo> getStatus(const RequestId& id) const;

    /// Результат, если запрос завершён; иначе ready=false.
    std::optional<RequestResultInfo> getResult(const RequestId& id) const;

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
                        const std::optional<std::string>& error,
                        bool setStarted, bool setFinished);
    void pruneOldSnapshotsLocked();
    std::string allocateHandlerId();

    QueryHandler handler_;
    AccessLogger* accessLogger_;
    std::size_t maxStoredRequests_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable finishedCv_;
    std::queue<Job> queue_;
    std::unordered_map<RequestId, RequestSnapshot> snapshots_;
    bool stop_ = false;

    std::atomic<std::uint64_t> nextHandlerSeq_{1};
    std::thread worker_;
};

}  // namespace executor
