#include "core/value.h"
#include <stdexcept>
#include <cctype>

namespace cw_db {

// Возвращает текстовое имя типа
const char* type_name(DataType t) {
    switch (t) {
        case DataType::Int: return "int";
        case DataType::Str: return "string";
    }
    return "?";
}

// Парсит строковое описание типа (регистр не важен)
bool parse_type(const std::string& s, DataType& out) {
    std::string low;
    low.reserve(s.size());
    for (char c : s) low += static_cast<char>(std::tolower((unsigned char)c));
    if (low == "int" || low == "integer") { out = DataType::Int; return true; }
    if (low == "string" || low == "str" || low == "text") { out = DataType::Str; return true; }
    return false;
}

// Фабрика NULL
Value Value::null() noexcept { return {}; }

// Фабрика целого
Value Value::of_int(int64_t v) noexcept {
    Value x; x.tag_ = Tag::Int; x.int_ = v; return x;
}

// Фабрика строки (интернируем через StringPool)
Value Value::of_str(const std::string& s) {
    Value x; x.tag_ = Tag::Str; x.str_id_ = StringPool::instance().intern(s); return x;
}

Value Value::of_str(std::string&& s) {
    Value x; x.tag_ = Tag::Str; x.str_id_ = StringPool::instance().intern(std::move(s)); return x;
}

Value Value::of_str_id(StringPool::Id id) noexcept {
    Value x; x.tag_ = Tag::Str; x.str_id_ = id; return x;
}

// Получить строку по id из пула
const std::string& Value::as_str() const {
    return StringPool::instance().get(str_id_);
}

bool Value::matches(DataType col_type) const noexcept {
    if (is_null()) return true;
    if (col_type == DataType::Int) return is_int();
    if (col_type == DataType::Str) return is_str();
    return false;
}

int Value::compare(const Value& other, bool& valid) const noexcept {
    // NULL и разные типы не сравниваем в SQL-смысле.
    if (is_null() || other.is_null()) { valid = false; return 0; }
    if (tag_ != other.tag_) { valid = false; return 0; }
    valid = true;
    if (tag_ == Tag::Int) {
        if (int_ < other.int_) return -1;
        if (int_ > other.int_) return 1;
        return 0;
    }
    // Интернированные строки сравниваем быстро по id, если они совпали.
    if (str_id_ == other.str_id_) return 0;
    const auto& a = as_str();
    const auto& b = other.as_str();
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

bool Value::operator==(const Value& other) const noexcept {
    if (tag_ != other.tag_) return false;
    switch (tag_) {
        case Tag::Null: return true;
        case Tag::Int: return int_ == other.int_;
        case Tag::Str: return str_id_ == other.str_id_;
    }
    return false;
}

}