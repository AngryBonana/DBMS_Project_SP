/**
 * @file executor_service.h
 * @brief Фасад подсистемы исполнителя для интеграции с сервером.
 *
 * ExecutorService объединяет асинхронную очередь, access-log и публичное API:
 *   - submit  — принять запрос, вернуть JSON (синхронный результат или GUID);
 *   - getStatus — статус по идентификатору (задание 6);
 *   - getResult — результат по идентификатору (задание 6).
 *
 * Длительные операции немедленно возвращают request_id; короткие SELECT
 * выполняются синхронно, но также попадают в журнал доступа (задание 7).
 */
#pragma once

#include "access_logger.h"
#include "async_executor.h"

#include <memory>
#include <string>

namespace executor {

/// Параметры работы сервиса (лимиты и пути).
struct ExecutorConfig {
    std::string accessLogPath = "access.log";
    /// Максимум хранимых снимков завершённых/активных запросов (защита от утечки памяти).
    std::size_t maxStoredRequests = 4096;
};

/**
 * Единая точка входа модуля executor для серверной части.
 * Владеет логгером и исполнителем; потокобезопасен на уровне AsyncExecutor.
 */
class ExecutorService {
public:
    explicit ExecutorService(QueryHandler handler,
                             ExecutorConfig config = {});

    /// Принять запрос: JSON с результатом (sync) или request_id (async).
    std::string submit(const std::string& query, const std::string& clientId);

    /// Статус запроса в формате JSON; при неизвестном id — JSON с ошибкой.
    std::string getStatusJson(const std::string& requestId) const;

    /// Результат запроса в формате JSON; если ещё не готов — ready=false.
    std::string getResultJson(const std::string& requestId) const;

    /// Низкоуровневый доступ к исполнителю (для тестов и расширений).
    AsyncExecutor& executor() { return *executor_; }
    const AsyncExecutor& executor() const { return *executor_; }

    AccessLogger& accessLogger() { return *logger_; }

private:
    std::string executeSync(const std::string& query,
                            const std::string& clientId);

    ExecutorConfig config_;
    QueryHandler handler_;
    std::unique_ptr<AccessLogger> logger_;
    std::unique_ptr<AsyncExecutor> executor_;
};

}  // namespace executor
