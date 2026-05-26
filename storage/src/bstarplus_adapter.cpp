#include "index/bstarplus_adapter.h"

#include <stdexcept>

namespace cw_db {

BStarPlusIndexAdapter::BStarPlusIndexAdapter(std::filesystem::path path, db::IndexKeyKind key_kind)
    : index_(std::move(path), key_kind)
    , key_kind_(key_kind) {
}

db::IndexKey BStarPlusIndexAdapter::to_index_key(const Value& value) {
    // Преобразуем storage::Value в ключ формата партнёрского индекса.
    if (value.is_int()) {
        return static_cast<std::int64_t>(value.as_int());
    }

    if (value.is_str()) {
        return value.as_str();
    }

    throw std::invalid_argument("BStarPlusIndexAdapter: NULL cannot be used as index key");
}

std::optional<RowId> BStarPlusIndexAdapter::find(const Value& key) const {
    const auto result = index_.find(to_index_key(key));
    if (!result.has_value()) {
        return std::nullopt;
    }
    // RowId у нас шире, поэтому приводим тип только на границе адаптера.
    return static_cast<RowId>(*result);
}

std::vector<RowId> BStarPlusIndexAdapter::range_search(const Value& begin, const Value& end) const {
    const auto raw = index_.range_search(to_index_key(begin), to_index_key(end));
    std::vector<RowId> result;
    result.reserve(raw.size());
    for (const auto row_id : raw) {
        result.push_back(static_cast<RowId>(row_id));
    }
    return result;
}

void BStarPlusIndexAdapter::insert(const Value& key, RowId row_id) {
    index_.insert(to_index_key(key), static_cast<std::size_t>(row_id));
}

bool BStarPlusIndexAdapter::erase(const Value& key) {
    return index_.erase(to_index_key(key));
}

} // namespace cw_db