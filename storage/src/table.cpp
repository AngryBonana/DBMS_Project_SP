#include "storage/table.h"
#include <stdexcept>
#include <fstream>
#include <cstdint>
#include <filesystem>
#include <algorithm>

namespace {
    using u8 = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;
    using i64 = std::int64_t;

    template<typename T>
    void write_bin(std::ofstream& out, T v) {
        out.write(reinterpret_cast<const char*>(&v), sizeof(T));
    }

    template<typename T>
    void read_bin(std::ifstream& in, T& outv) {
        in.read(reinterpret_cast<char*>(&outv), sizeof(T));
        if (!in) {
            throw std::runtime_error("Table::load: truncated or corrupt file");
        }
    }
}

namespace cw_db {

// Реализация операций над таблицей: вставка строк и персистентность
// Краткие комментарии на русском рядом с ключевыми участками.

namespace {
    constexpr std::uint32_t kMagic = 0x42445442; // "BDTB"
    constexpr std::uint32_t kVersionV3 = 3;
}

int Table::require_column(const std::string& name) const {
    auto index = schema_.find_index(name);
    if (!index.has_value()) {
        throw std::runtime_error("Table::require_column: unknown column '" + name + "'");
    }
    return static_cast<int>(*index);
}

Row Table::materialize(const std::vector<int>& column_positions, std::vector<Value> values) const {
    if (column_positions.size() != values.size()) {
        throw std::invalid_argument("Table::materialize: columns and values count mismatch");
    }

    Row row;
    row.values.resize(schema_.column_count(), Value::null());

    std::vector<bool> assigned(schema_.column_count(), false);
    for (std::size_t i = 0; i < column_positions.size(); ++i) {
        const int pos = column_positions[i];
        if (pos < 0 || static_cast<std::size_t>(pos) >= schema_.column_count()) {
            throw std::out_of_range("Table::materialize: column index out of range");
        }
        if (assigned[static_cast<std::size_t>(pos)]) {
            throw std::invalid_argument("Table::materialize: duplicate column index");
        }
        assigned[static_cast<std::size_t>(pos)] = true;
        row.values[static_cast<std::size_t>(pos)] = std::move(values[i]);
    }

    return row;
}

bool Table::value_exists_in_column(std::size_t column, const Value& value,
                                   std::optional<RowId> exclude_id) const {
    // Если индекс уже подключён, используем его как быстрый путь проверки.
    if (auto index_it = indexes_.find(column); index_it != indexes_.end()) {
        const auto found = index_it->second.index->find(value);
        if (!found.has_value()) {
            return false;
        }
        return !exclude_id.has_value() || *found != *exclude_id;
    }

    // Иначе делаем обычный линейный проход по строкам таблицы.
    for (const auto& [row_id, row] : rows_) {
        if (exclude_id.has_value() && row_id == *exclude_id) {
            continue;
        }
        bool valid = true;
        if (row.values.at(column).compare(value, valid) == 0 && valid) {
            return true;
        }
    }

    return false;
}

void Table::validate(const Row& row, std::optional<RowId> existing_id) const {
    if (row.values.size() != schema_.column_count()) {
        throw std::invalid_argument("Table::validate: row arity mismatch");
    }

    for (std::size_t i = 0; i < schema_.column_count(); ++i) {
        const auto& col = schema_.columns().at(i);
        const auto& value = row.values.at(i);

        if (value.is_null()) {
            if (col.not_null || col.indexed) {
                throw std::invalid_argument("Table::validate: NULL not allowed in column '" + col.name + "'");
            }
            continue;
        }

        if (!value.matches(col.type)) {
            throw std::invalid_argument("Table::validate: type mismatch for column '" + col.name + "'");
        }

        if (col.indexed && value_exists_in_column(i, value, existing_id)) {
            throw std::invalid_argument("Table::validate: duplicate value in indexed column '" + col.name + "'");
        }
    }
}

RowId Table::insert(Row row) {
    validate(row);

    const RowId row_id = next_id_++;
    rows_.emplace(row_id, std::move(row));
    order_.push_back(row_id);
    notify_indexes_insert(row_id, rows_.at(row_id));
    return row_id;
}

void Table::update(RowId id, std::size_t column, const Value& new_value) {
    auto it = rows_.find(id);
    if (it == rows_.end()) {
        throw std::out_of_range("Table::update: row id not found");
    }
    if (column >= schema_.column_count()) {
        throw std::out_of_range("Table::update: column out of range");
    }

    Row updated = it->second;
    const Value old_value = updated.values.at(column);
    updated.values.at(column) = new_value;
    validate(updated, id);

    notify_indexes_update(id, column, old_value, new_value);
    it->second = std::move(updated);
}

void Table::erase(RowId id) {
    auto it = rows_.find(id);
    if (it == rows_.end()) {
        throw std::out_of_range("Table::erase: row id not found");
    }

    notify_indexes_erase(it->second);
    rows_.erase(it);
    auto pos = std::find(order_.begin(), order_.end(), id);
    if (pos != order_.end()) {
        order_.erase(pos);
    }
}

void Table::notify_indexes_insert(RowId row_id, const Row& row) {
    for (const auto& [column, binding] : indexes_) {
        if (column < row.values.size()) {
            binding.index->insert(row.values.at(column), row_id);
        }
    }
}

void Table::notify_indexes_update(RowId row_id, std::size_t column, const Value& old_value, const Value& new_value) {
    auto it = indexes_.find(column);
    if (it == indexes_.end()) {
        return;
    }

    // Сначала убираем старый ключ, затем добавляем новый.
    it->second.index->erase(old_value);
    it->second.index->insert(new_value, row_id);
}

void Table::notify_indexes_erase(const Row& row) {
    // Снимаем все ключи строки со всех подключённых индексов.
    for (const auto& [column, binding] : indexes_) {
        if (column < row.values.size()) {
            binding.index->erase(row.values.at(column));
        }
    }
}

// Вставка одной строки: проверяем типы, подставляем default/NULL и соблюдаем NOT NULL
void Table::insert_row(const std::vector<std::optional<Value>>& values) {
    if (values.size() > schema_.column_count()) {
        throw std::invalid_argument("Table::insert_row: too many values");
    }

    Row row;
    row.values.reserve(schema_.column_count());

    for (std::size_t i = 0; i < schema_.column_count(); ++i) {
        const auto& col = schema_.columns().at(i);

        if (i < values.size() && values[i].has_value()) {
            row.values.push_back(*values[i]);
            continue;
        }

        if (col.default_value.has_value()) {
            row.values.push_back(*col.default_value);
        } else if (col.not_null) {
            throw std::invalid_argument("Table::insert_row: missing NOT NULL value for column '" + col.name + "'");
        } else {
            row.values.push_back(Value::null());
        }
    }

    insert(std::move(row));
}

// --- Персистентность (сохранение/загрузка) ---

// Сохраняем таблицу в бинарный файл. Для атомарности: пишем во временный файл, затем переименовываем.
void Table::save(const std::string& path) const {
    std::filesystem::path p(path);
    auto tmp = p;
    tmp += ".tmp";

    std::ofstream out(tmp.string(), std::ios::binary);
    if (!out) throw std::runtime_error("Table::save: cannot open temporary file");

    // Заголовок: magic 'CWDB' и версия формата.
    write_bin<std::uint32_t>(out, kMagic);
    write_bin<std::uint32_t>(out, kVersionV3);

    // Имя таблицы.
    u16 name_len = static_cast<u16>(name_.size());
    write_bin<u16>(out, name_len);
    out.write(name_.data(), name_len);

    // Сериализуем StringPool (чтобы затем ссылаться на строки по id)
    StringPool::instance().serialize(out);

    // Описание колонок.
    u32 col_count = static_cast<u32>(schema_.column_count());
    write_bin<u32>(out, col_count);
    for (const auto& col : schema_.columns()) {
        u16 name_len = static_cast<u16>(col.name.size());
        write_bin<u16>(out, name_len);
        out.write(col.name.data(), name_len);
        u8 type = (col.type == DataType::Int) ? 1 : 2;
        write_bin<u8>(out, type);
        u8 flags = (col.not_null ? 1u : 0u) | (col.indexed ? 2u : 0u);
        write_bin<u8>(out, flags);

        // Наличие значения по умолчанию
        u8 has_def = col.default_value.has_value() ? 1u : 0u;
        write_bin<u8>(out, has_def);
        if (has_def) {
            const Value& dv = *col.default_value;
            if (dv.is_null()) {
                write_bin<u8>(out, (u8)0);
            } else if (dv.is_int()) {
                write_bin<u8>(out, (u8)1);
                i64 v = dv.as_int(); write_bin<i64>(out, v);
            } else {
                // Строка уже лежит в пуле, поэтому сохраняем только её id.
                write_bin<u8>(out, (u8)2);
                u32 id = static_cast<u32>(dv.as_str_id()); write_bin<u32>(out, id);
            }
        }
    }

    // Строки
    u64 rows = static_cast<u64>(order_.size());
    write_bin<u64>(out, rows);
    for (const auto row_id : order_) {
        write_bin<RowId>(out, row_id);
        for (const auto& v : rows_.at(row_id).values) {
            if (v.is_null()) { write_bin<u8>(out, (u8)0); }
            else if (v.is_int()) { write_bin<u8>(out, (u8)1); i64 x = v.as_int(); write_bin<i64>(out, x); }
            else { write_bin<u8>(out, (u8)2); u32 id = static_cast<u32>(v.as_str_id()); write_bin<u32>(out, id); }
        }
    }

    out.close();

    // Попытка атомарного переименования временного файла в целевой
    std::error_code ec;
    std::filesystem::rename(tmp, p, ec);
    if (ec) {
        // при неудаче удаляем временный файл и сообщаем ошибку
        std::filesystem::remove(tmp, ec);
        throw std::runtime_error("Table::save: cannot rename temporary file to target");
    }
}

Table Table::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Table::load: cannot open file");

    std::string table_name = std::filesystem::path(path).stem().string();

    std::uint32_t magic;
    read_bin<std::uint32_t>(in, magic);
    if (magic != kMagic) throw std::runtime_error("Table::load: bad magic");
    std::uint32_t version;
    read_bin<std::uint32_t>(in, version);

    if (version != kVersionV3) {
        throw std::runtime_error("Table::load: unsupported version");
    }

    u16 name_len;
    read_bin<u16>(in, name_len);
    table_name.resize(name_len);
    in.read(table_name.data(), name_len);

    u32 pool_count;
    read_bin<u32>(in, pool_count);
    std::vector<StringPool::Id> id_map;
    id_map.resize(static_cast<std::size_t>(pool_count) + 1);
    id_map[0] = StringPool::kInvalid;
    for (u32 i = 1; i <= pool_count; ++i) {
        u32 len;
        read_bin<u32>(in, len);
        std::string s;
        s.resize(len);
        if (len > 0) {
            in.read(&s[0], len);
        }
        id_map[i] = StringPool::instance().intern(std::move(s));
    }

    u32 col_count;
    read_bin<u32>(in, col_count);
    std::vector<ColumnDef> cols;
    cols.reserve(col_count);
    for (u32 i = 0; i < col_count; ++i) {
        u16 column_name_len;
        read_bin<u16>(in, column_name_len);
        std::string name;
        name.resize(column_name_len);
        in.read(&name[0], column_name_len);
        u8 type;
        read_bin<u8>(in, type);
        u8 flags;
        read_bin<u8>(in, flags);
        u8 has_def;
        read_bin<u8>(in, has_def);
        ColumnDef col;
        col.name = name;
        col.type = (type == 1 ? DataType::Int : DataType::Str);
        col.not_null = (flags & 1);
        col.indexed = (flags & 2);
        if (has_def) {
            u8 dt;
            read_bin<u8>(in, dt);
            if (dt == 0) {
                col.default_value = Value::null();
            } else if (dt == 1) {
                i64 v;
                read_bin<i64>(in, v);
                col.default_value = Value::of_int(v);
            } else {
                u32 id;
                read_bin<u32>(in, id);
                col.default_value = Value::of_str_id(id_map.at(id));
            }
        }
        cols.push_back(std::move(col));
    }

    TableSchema schema(std::move(cols));
    Table t(table_name, schema);

    u64 rows;
    read_bin<u64>(in, rows);
    for (u64 r = 0; r < rows; ++r) {
        RowId row_id;
        read_bin<RowId>(in, row_id);
        std::vector<std::optional<Value>> vals;
        vals.reserve(schema.column_count());
        for (std::size_t c = 0; c < schema.column_count(); ++c) {
            u8 tag;
            read_bin<u8>(in, tag);
            if (tag == 0) {
                vals.push_back(std::nullopt);
            } else if (tag == 1) {
                i64 v;
                read_bin<i64>(in, v);
                vals.push_back(Value::of_int(v));
            } else {
                u32 id;
                read_bin<u32>(in, id);
                vals.push_back(Value::of_str_id(id_map.at(id)));
            }
        }

        Row row;
        row.values.reserve(schema.column_count());
        for (const auto& val : vals) {
            row.values.push_back(val.value_or(Value::null()));
        }
        t.rows_.emplace(row_id, std::move(row));
        t.order_.push_back(row_id);
    }

    if (!t.order_.empty()) {
        t.next_id_ = std::max<RowId>(*std::max_element(t.order_.begin(), t.order_.end()) + 1, t.next_id_);
    }

    return t;
}

void Table::attach_index(std::size_t column, IIndexPtr index, bool rebuild) {
    if (column >= schema_.column_count()) {
        throw std::out_of_range("Table::attach_index: column out of range");
    }
    if (!index) {
        throw std::invalid_argument("Table::attach_index: index must not be null");
    }

    indexes_[column] = IndexBinding{std::move(index)};

    if (rebuild) {
        auto& binding = indexes_.at(column);
        for (const auto row_id : order_) {
            binding.index->insert(rows_.at(row_id).values.at(column), row_id);
        }
    }
}

void Table::detach_index(std::size_t column) {
    indexes_.erase(column);
}

bool Table::has_index(std::size_t column) const {
    return indexes_.find(column) != indexes_.end();
}

IIndex* Table::index_for(std::size_t column) const {
    auto it = indexes_.find(column);
    return it == indexes_.end() ? nullptr : it->second.index.get();
}

} 