#include "core/string_pool.h"
#include <stdexcept>

namespace cw_db {

StringPool& StringPool::instance() {
    static StringPool pool;
    return pool;
}

StringPool::StringPool() {
    // резервируем id 0
    by_id_.push_back("");
}

StringPool::Id StringPool::intern(const std::string& s) {
    std::lock_guard lock(mu_);
    auto it = by_value_.find(s);
    if (it != by_value_.end()) return it->second;
    Id id = static_cast<Id>(by_id_.size());
    by_id_.push_back(s);
    by_value_.emplace(by_id_.back(), id);
    return id;
}

StringPool::Id StringPool::intern(std::string&& s) {
    std::lock_guard lock(mu_);
    auto it = by_value_.find(s);
    if (it != by_value_.end()) return it->second;
    Id id = static_cast<Id>(by_id_.size());
    by_id_.push_back(std::move(s));
    by_value_.emplace(by_id_.back(), id);
    return id;
}

const std::string& StringPool::get(Id id) const {
    std::lock_guard lock(mu_);
    if (id == kInvalid || id >= by_id_.size())
        throw std::runtime_error("StringPool: invalid id");
    return by_id_[id];
}

}