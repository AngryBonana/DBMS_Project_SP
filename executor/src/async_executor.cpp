/**
 * @file async_executor.cpp
 * @brief Реализация асинхронного исполнителя.
 *
 * Управляет фоновым рабочим потоком, очередью задач (Job), состоянием
 * каждого запроса (snapshots_). При получении задачи обновляет статус,
 * вызывает переданный обработчик, фиксирует время выполнения и результат.
 * При наличии логгера записывает информацию о выполненном запросе.
 * Уведомляет ожидающие потоки через condition_variable.
 */
#include "async_executor.h"

#include <utility>

namespace executor {

AsyncExecutor::AsyncExecutor(QueryHandler handler, AccessLogger* accessLogger)
    : handler_(std::move(handler)), accessLogger_(accessLogger) {
    worker_ = std::thread([this]() { workerLoop(); });
}

AsyncExecutor::~AsyncExecutor() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

RequestId AsyncExecutor::submit(const std::string& query,
                                const std::string& clientId) {
    const RequestId id = generateRequestId();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_[id] = RequestSnapshot{id, RequestStatus::Pending, {}, {}};
        queue_.push(Job{id, clientId, query});
    }

    cv_.notify_one();
    return id;
}

std::optional<RequestSnapshot> AsyncExecutor::getSnapshot(
    const RequestId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = snapshots_.find(id);
    if (it == snapshots_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool AsyncExecutor::waitUntilFinished(const RequestId& id,
                                      std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return finishedCv_.wait_for(lock, timeout, [this, &id]() {
        const auto it = snapshots_.find(id);
        if (it == snapshots_.end()) {
            return true;
        }
        const auto status = it->second.status;
        return status == RequestStatus::Completed ||
               status == RequestStatus::Failed;
    });
}

void AsyncExecutor::workerLoop() {
    while (true) {
        Job job;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return stop_ || !queue_.empty(); });

            if (stop_ && queue_.empty()) {
                return;
            }

            job = std::move(queue_.front());
            queue_.pop();
        }

        processJob(job);
    }
}

void AsyncExecutor::processJob(const Job& job) {
    updateSnapshot(job.id, RequestStatus::Running, std::nullopt,
                   std::nullopt);

    const auto startTime = std::chrono::system_clock::now();
    const std::string handlerId = job.id;

    int statusCode = 0;
    std::string statusMessage = "OK";
    std::optional<std::string> result;
    std::optional<std::string> error;

    try {
        result = handler_(job.query);
    } catch (const std::exception& ex) {
        statusCode = 1;
        statusMessage = ex.what();
        error = statusMessage;
    }

    const auto endTime = std::chrono::system_clock::now();

    if (error) {
        updateSnapshot(job.id, RequestStatus::Failed, std::nullopt, error);
    } else {
        updateSnapshot(job.id, RequestStatus::Completed, result, std::nullopt);
    }

    if (accessLogger_) {
        accessLogger_->logRequest(AccessLogEntry{
            job.query,
            job.clientId,
            handlerId,
            startTime,
            endTime,
            statusCode,
            statusMessage,
        });
    }

    finishedCv_.notify_all();
}

void AsyncExecutor::updateSnapshot(const RequestId& id, RequestStatus status,
                                   const std::optional<std::string>& result,
                                   const std::optional<std::string>& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& snapshot = snapshots_[id];
    snapshot.id = id;
    snapshot.status = status;
    snapshot.result = result;
    snapshot.error = error;
}

}  // namespace executor