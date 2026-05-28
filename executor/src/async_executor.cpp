/**
 * @file async_executor.cpp
 * @brief Реализация асинхронного исполнителя.
 */
#include "async_executor.h"

#include <utility>

namespace executor {

namespace {

RequestStatusInfo toStatusInfo(const RequestSnapshot& snapshot) {
    return RequestStatusInfo{
        snapshot.id,
        snapshot.status,
        snapshot.submittedAt,
        snapshot.startedAt,
        snapshot.finishedAt,
        std::nullopt,
    };
}

RequestResultInfo toResultInfo(const RequestSnapshot& snapshot) {
    RequestResultInfo info;
    info.id = snapshot.id;
    info.status = snapshot.status;

    if (snapshot.status == RequestStatus::Completed) {
        info.ready = true;
        info.result = snapshot.result;
    } else if (snapshot.status == RequestStatus::Failed) {
        info.ready = true;
        info.error = snapshot.error;
    } else {
        info.ready = false;
    }

    return info;
}

}  // namespace

AsyncExecutor::AsyncExecutor(QueryHandler handler, AccessLogger* accessLogger,
                             std::size_t maxStoredRequests)
    : handler_(std::move(handler)),
      accessLogger_(accessLogger),
      maxStoredRequests_(maxStoredRequests) {
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
    const auto submittedAt = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pruneOldSnapshotsLocked();

        RequestSnapshot snapshot;
        snapshot.id = id;
        snapshot.status = RequestStatus::Pending;
        snapshot.submittedAt = submittedAt;
        snapshots_[id] = std::move(snapshot);

        // Фоновый worker забирает задания из queue_ и обновляет snapshots_.
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

std::optional<RequestStatusInfo> AsyncExecutor::getStatus(
    const RequestId& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = snapshots_.find(id);
    if (it == snapshots_.end()) {
        return std::nullopt;
    }
    auto info = toStatusInfo(it->second);
    if (info.status == RequestStatus::Pending) {
        info.queuePosition = getQueuePositionLocked(id);
    }
    return info;
}

std::optional<RequestResultInfo> AsyncExecutor::getResult(
    const RequestId& id) const {
    const auto snapshot = getSnapshot(id);
    if (!snapshot) {
        return std::nullopt;
    }
    return toResultInfo(*snapshot);
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
    updateSnapshot(job.id, RequestStatus::Running, std::nullopt, std::nullopt,
                   true, false);

    const auto startTime = std::chrono::system_clock::now();
    const std::string handlerId = allocateHandlerId();

    int statusCode = static_cast<int>(ReturnCode::Ok);
    std::string statusMessage = "OK";
    std::optional<std::string> result;
    std::optional<std::string> error;

    try {
        result = handler_(job.query);
    } catch (const std::exception& ex) {
        statusCode = static_cast<int>(ReturnCode::Error);
        statusMessage = ex.what();
        error = statusMessage;
    }

    const auto endTime = std::chrono::system_clock::now();

    if (error) {
        updateSnapshot(job.id, RequestStatus::Failed, std::nullopt, error,
                       false, true);
    } else {
        updateSnapshot(job.id, RequestStatus::Completed, result, std::nullopt,
                       false, true);
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
                                   const std::optional<std::string>& error,
                                   bool setStarted, bool setFinished) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& snapshot = snapshots_[id];
    snapshot.id = id;
    snapshot.status = status;
    snapshot.result = result;
    snapshot.error = error;

    if (setStarted) {
        snapshot.startedAt = std::chrono::system_clock::now();
    }
    if (setFinished) {
        snapshot.finishedAt = std::chrono::system_clock::now();
    }
}

void AsyncExecutor::pruneOldSnapshotsLocked() {
    if (snapshots_.size() < maxStoredRequests_) {
        return;
    }

    // Удаляем самый старый завершённый снимок, чтобы не раздувать память.
    auto oldestFinished = snapshots_.end();
    for (auto it = snapshots_.begin(); it != snapshots_.end(); ++it) {
        const bool finished =
            it->second.status == RequestStatus::Completed ||
            it->second.status == RequestStatus::Failed;
        if (!finished) {
            continue;
        }
        if (oldestFinished == snapshots_.end() ||
            (it->second.finishedAt && oldestFinished->second.finishedAt &&
             *it->second.finishedAt < *oldestFinished->second.finishedAt)) {
            oldestFinished = it;
        }
    }

    if (oldestFinished != snapshots_.end()) {
        snapshots_.erase(oldestFinished);
    }
}

std::string AsyncExecutor::allocateHandlerId() {
    return "handler-" + std::to_string(nextHandlerSeq_.fetch_add(1));
}

std::optional<std::size_t> AsyncExecutor::getQueuePositionLocked(
    const RequestId& id) const {
    // std::queue не итерируется напрямую, поэтому используем копию для оценки позиции.
    std::queue<Job> localCopy = queue_;
    std::size_t index = 0;
    while (!localCopy.empty()) {
        if (localCopy.front().id == id) {
            return index;
        }
        localCopy.pop();
        ++index;
    }
    return std::nullopt;
}

}  // namespace executor
