#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "core/value.h"

namespace cw_db {

using RowId = std::size_t;

// Минимальный контракт для индекса: реализацию даст коллега.
class IIndex {
public:
    virtual ~IIndex() = default;

    // Добавить ключ и ссылку на строку.
    virtual void insert(const Value& key, RowId row_id) = 0;

    // Удалить ключ и ссылку на строку.
    virtual void erase(const Value& key, RowId row_id) = 0;

    // Найти строки по ключу.
    [[nodiscard]] virtual std::vector<RowId> find(const Value& key) const = 0;

    // Очистить индекс.
    virtual void clear() = 0;
};

using IIndexPtr = std::shared_ptr<IIndex>;

} // namespace cw_db