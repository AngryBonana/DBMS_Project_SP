#pragma once
#include <cstdint>
#include <string>
#include "core/string_pool.h"

namespace cw_db {

// Тип данных столбца
enum class DataType : uint8_t { Int = 1, Str = 2 };

// Имя типа ("int" / "string")
const char* type_name(DataType t);
// Парсит строковое имя типа в DataType
bool parse_type(const std::string& s, DataType& out);

class Value {
public:
    enum class Tag : uint8_t { Null = 0, Int = 1, Str = 2 };

    // Конструктор по умолчанию -> NULL
    Value() noexcept : tag_(Tag::Null), int_(0) {}

    static Value null() noexcept;
    static Value of_int(int64_t v) noexcept;
    static Value of_str(const std::string& s);
    static Value of_str(std::string&& s);

    // Доступ к тегу (Null/Int/Str)
    Tag tag() const noexcept { return tag_; }
    bool is_null() const noexcept { return tag_ == Tag::Null; }
    bool is_int() const noexcept { return tag_ == Tag::Int; }
    bool is_str() const noexcept { return tag_ == Tag::Str; }

    // Получить числовое значение (только для Int)
    int64_t as_int() const noexcept { return int_; }
    // Получить строку через StringPool (только для Str)
    const std::string& as_str() const;

    // Совместимость значения с типом столбца (NULL совместим с любым)
    bool matches(DataType col_type) const noexcept;

    // Трёхзначное сравнение: -1, 0, 1.
    // valid=false если присутствует NULL или типы разные
    int compare(const Value& other, bool& valid) const noexcept;

    bool operator==(const Value& other) const noexcept;
    bool operator!=(const Value& other) const noexcept { return !(*this == other); }

private:
    Tag tag_;
    union {
        int64_t int_;
        StringPool::Id str_id_;
    };
};

}