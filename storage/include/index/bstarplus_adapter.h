#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "index.h"

#include "bstarplus_tree_index.h"

namespace cw_db {

// Тонкий мост между storage и партнёрской реализацией B*+-tree.
class BStarPlusIndexAdapter final : public IIndex {
public:
    explicit BStarPlusIndexAdapter(std::filesystem::path path, db::IndexKeyKind key_kind);

    [[nodiscard]] std::optional<RowId> find(const Value& key) const override;
    [[nodiscard]] std::vector<RowId> range_search(const Value& begin, const Value& end) const override;
    void insert(const Value& key, RowId row_id) override;
    bool erase(const Value& key) override;

private:
    static db::IndexKey to_index_key(const Value& value);

    mutable db::BStarPlusIndex index_;
    db::IndexKeyKind key_kind_;
};

} // namespace cw_db