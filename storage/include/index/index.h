#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "core/value.h"

namespace cw_db {

using RowId = std::uint64_t;

// Минимальный контракт для индекса: уникальный ключ -> один RowId.
class IIndex {
public:
    virtual ~IIndex() = default;

    // Найти строку по ключу.
    [[nodiscard]] virtual std::optional<RowId> find(const Value& key) const = 0;

    // Диапазон для BETWEEN / range-scan.
    [[nodiscard]] virtual std::vector<RowId> range_search(const Value& begin, const Value& end) const = 0;

    // Добавить уникальный ключ и ссылку на строку.
    virtual void insert(const Value& key, RowId row_id) = 0;

    // Удалить ключ.
    virtual bool erase(const Value& key) = 0;
};

using IIndexPtr = std::shared_ptr<IIndex>;

} // namespace cw_db