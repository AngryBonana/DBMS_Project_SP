#include "storage/table.h"
#include <stdexcept>
#include <fstream>
#include <cstdint>
#include <filesystem>

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

// Вставка одной строки: проверяем типы, подставляем default/NULL и соблюдаем NOT NULL
void Table::insert_row(const std::vector<std::optional<Value>>& values) {
    const auto cols = schema_.column_count();
    if (values.size() > cols) {
        throw std::invalid_argument("Table::insert_row: too many values");
    }

    std::vector<Value> row;
    row.reserve(cols);

    for (std::size_t i = 0; i < cols; ++i) {
        const auto& col = schema_.columns()[i];

        if (i < values.size() && values[i].has_value()) {
            const Value& v = *values[i];
            if (!v.matches(col.type)) {
                throw std::invalid_argument("Table::insert_row: type mismatch for column '" + col.name + "'");
            }
            row.push_back(v);
        } else {
            // отсутствующее значение — используем default, NULL или ошибку при NOT NULL
            if (col.default_value.has_value()) {
                row.push_back(*col.default_value);
            } else {
                if (col.not_null) {
                    throw std::invalid_argument("Table::insert_row: missing NOT NULL value for column '" + col.name + "'");
                }
                row.push_back(Value::null());
            }
        }
    }

    rows_.push_back(row);
    notify_indexes_insert(rows_.size() - 1, rows_.back());
}

// --- Персистентность (сохранение/загрузка) ---

// Сохраняем таблицу в бинарный файл. Для атомарности: пишем во временный файл, затем переименовываем.
void Table::save(const std::string& path) const {
    std::filesystem::path p(path);
    auto tmp = p;
    tmp += ".tmp";

    std::ofstream out(tmp.string(), std::ios::binary);
    if (!out) throw std::runtime_error("Table::save: cannot open temporary file");

    // Заголовок: magic 'CWDB', версия u16
    const char magic[4] = {'C','W','D','B'};
    out.write(magic, 4);
    u16 version = 2; // version 2: includes serialized StringPool and id-based string refs
    write_bin<u16>(out, version);

    // Сериализуем StringPool (чтобы затем ссылаться на строки по id)
    StringPool::instance().serialize(out);

    // Описание колонок
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
                // Для версии 2 сохраняем id строки в пуле
                write_bin<u8>(out, (u8)2);
                u32 id = static_cast<u32>(dv.as_str_id()); write_bin<u32>(out, id);
            }
        }
    }

    // Строки
    u64 rows = static_cast<u64>(rows_.size()); write_bin<u64>(out, rows);
    for (const auto& row : rows_) {
        for (const auto& v : row) {
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

    char magic[4]; in.read(magic,4);
    if (magic[0]!='C' || magic[1]!='W' || magic[2]!='D' || magic[3]!='B') throw std::runtime_error("Table::load: bad magic");
    u16 version; read_bin<u16>(in, version);
    if (version == 1) {
        // старый формат: пул строк не хранится; строки записаны инлайново
        u32 col_count; read_bin<u32>(in, col_count);
        std::vector<ColumnDef> cols; cols.reserve(col_count);
        for (u32 i=0;i<col_count;++i) {
            u16 name_len; read_bin<u16>(in, name_len);
            std::string name; name.resize(name_len);
            in.read(&name[0], name_len);
            u8 type; read_bin<u8>(in, type);
            u8 flags; read_bin<u8>(in, flags);
            u8 has_def; read_bin<u8>(in, has_def);
            ColumnDef col; col.name = name; col.type = (type==1?DataType::Int:DataType::Str);
            col.not_null = (flags & 1); col.indexed = (flags & 2);
            if (has_def) {
                u8 dt; read_bin<u8>(in, dt);
                if (dt==0) { col.default_value = Value::null(); }
                else if (dt==1) { i64 v; read_bin<i64>(in, v); col.default_value = Value::of_int(v); }
                else { u32 l; read_bin<u32>(in, l); std::string s; s.resize(l); in.read(&s[0], l); col.default_value = Value::of_str(std::move(s)); }
            }
            cols.push_back(std::move(col));
        }

        TableSchema schema(std::move(cols));
        Table t(schema);

        u64 rows; read_bin<u64>(in, rows);
        for (u64 r=0;r<rows;++r) {
            std::vector<std::optional<Value>> vals;
            vals.reserve(schema.column_count());
            for (std::size_t c=0;c<schema.column_count();++c) {
                u8 tag; read_bin<u8>(in, tag);
                if (tag==0) { vals.push_back(std::nullopt); }
                else if (tag==1) { i64 v; read_bin<i64>(in, v); vals.push_back(Value::of_int(v)); }
                else { u32 l; read_bin<u32>(in, l); std::string s; s.resize(l); in.read(&s[0], l); vals.push_back(Value::of_str(std::move(s))); }
            }
            t.insert_row(vals);
        }
        return t;
    } else if (version >= 2) {
        // новый формат: сначала сериализован пул строк, затем строки по id
        // Пул не перезаписываем целиком, а подмешиваем строки в глобальный StringPool.
        u32 pool_count; read_bin<u32>(in, pool_count);
        std::vector<StringPool::Id> id_map;
        id_map.resize(static_cast<std::size_t>(pool_count) + 1);
        id_map[0] = StringPool::kInvalid;
        for (u32 i = 1; i <= pool_count; ++i) {
            u32 len; read_bin<u32>(in, len);
            std::string s;
            s.resize(len);
            if (len > 0) {
                in.read(&s[0], len);
            }
            id_map[i] = StringPool::instance().intern(std::move(s));
        }

        u32 col_count; read_bin<u32>(in, col_count);
        std::vector<ColumnDef> cols; cols.reserve(col_count);
        for (u32 i=0;i<col_count;++i) {
            u16 name_len; read_bin<u16>(in, name_len);
            std::string name; name.resize(name_len);
            in.read(&name[0], name_len);
            u8 type; read_bin<u8>(in, type);
            u8 flags; read_bin<u8>(in, flags);
            u8 has_def; read_bin<u8>(in, has_def);
            ColumnDef col; col.name = name; col.type = (type==1?DataType::Int:DataType::Str);
            col.not_null = (flags & 1); col.indexed = (flags & 2);
            if (has_def) {
                u8 dt; read_bin<u8>(in, dt);
                if (dt==0) { col.default_value = Value::null(); }
                else if (dt==1) { i64 v; read_bin<i64>(in, v); col.default_value = Value::of_int(v); }
                else { u32 id; read_bin<u32>(in, id); col.default_value = Value::of_str_id(id_map.at(id)); }
            }
            cols.push_back(std::move(col));
        }

        TableSchema schema(std::move(cols));
        Table t(schema);

        u64 rows; read_bin<u64>(in, rows);
        for (u64 r=0;r<rows;++r) {
            std::vector<std::optional<Value>> vals;
            vals.reserve(schema.column_count());
            for (std::size_t c=0;c<schema.column_count();++c) {
                u8 tag; read_bin<u8>(in, tag);
                if (tag==0) { vals.push_back(std::nullopt); }
                else if (tag==1) { i64 v; read_bin<i64>(in, v); vals.push_back(Value::of_int(v)); }
                else { u32 id; read_bin<u32>(in, id); vals.push_back(Value::of_str_id(id_map.at(id))); }
            }
            t.insert_row(vals);
        }

        return t;
    } else {
        throw std::runtime_error("Table::load: unsupported version");
    }
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
        for (std::size_t row_id = 0; row_id < rows_.size(); ++row_id) {
            binding.index->insert(rows_[row_id].at(column), row_id);
        }
    }
}

void Table::detach_index(std::size_t column) {
    indexes_.erase(column);
}

bool Table::has_index(std::size_t column) const {
    return indexes_.find(column) != indexes_.end();
}

void Table::notify_indexes_insert(std::size_t row_id, const std::vector<Value>& row) {
    for (auto& [column, binding] : indexes_) {
        if (column < row.size()) {
            binding.index->insert(row.at(column), row_id);
        }
    }
}

} 