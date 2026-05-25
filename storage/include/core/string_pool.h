#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace cw_db {

class StringPool {
public:
    using Id = uint32_t;
    static constexpr Id kInvalid = 0;

    static StringPool& instance();

    // Зарегистрировать строку и вернуть её id (если уже есть — вернуть существующий)
    Id intern(const std::string& s);
    Id intern(std::string&& s);
    // Получить строку по id (бросает при неверном id)
    const std::string& get(Id id) const;

    // Сериализация пула строк в поток (не включает id 0)
    void serialize(std::ostream& out) const;
    // Десериализация пула из потока (перезапишет текущее содержимое)
    void deserialize(std::istream& in);

private:
    StringPool();
    mutable std::mutex mu_;
    std::unordered_map<std::string, Id> by_value_;
    std::vector<std::string> by_id_; // by_id_[0] reserved
};

}