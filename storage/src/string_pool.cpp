#include "core/string_pool.h"
#include <istream>
#include <ostream>
#include <cstdint>
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
    // Сначала ищем уже существующую строку, чтобы не плодить дубликаты.
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


void StringPool::serialize(std::ostream& out) const {
    std::lock_guard lock(mu_);
    // number of entries excluding reserved 0
    uint32_t count = static_cast<uint32_t>(by_id_.size() > 0 ? by_id_.size() - 1 : 0);
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (uint32_t i = 1; i <= count; ++i) {
        const std::string& s = by_id_[i];
        uint32_t len = static_cast<uint32_t>(s.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        out.write(s.data(), len);
    }
}

void StringPool::deserialize(std::istream& in) {
    std::lock_guard lock(mu_);
    by_value_.clear();
    by_id_.clear();
    by_id_.push_back(""); // reserve id 0
    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    // Восстанавливаем пул в том же порядке, в каком он был сохранён.
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string s; s.resize(len);
        in.read(&s[0], len);
        Id id = static_cast<Id>(by_id_.size());
        by_id_.push_back(std::move(s));
        by_value_.emplace(by_id_.back(), id);
    }
}

} 