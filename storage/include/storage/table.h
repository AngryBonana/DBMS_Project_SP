#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <stdexcept>
#include "sql/schema.h"
#include "index/index.h"

namespace cw_db {

// Класс таблицы: хранит схему и ряд значений (materialized rows)
class Table {
public:
    explicit Table(const TableSchema& schema) : schema_(schema) {}

    // Вставляет одну строку значений с возможными пропусками (std::nullopt)
    // Значения соответствуют порядку колонок в схеме
    void insert_row(const std::vector<std::optional<Value>>& values);

    // Получить материализованную строку
    const std::vector<Value>& get_row(std::size_t idx) const {
        return rows_.at(idx);
    }

    std::size_t row_count() const noexcept { return rows_.size(); }

    const TableSchema& schema() const noexcept { return schema_; }

    // Сохранить таблицу в бинарный файл
    void save(const std::string& path) const;

    // Загрузить таблицу из бинарного файла
    static Table load(const std::string& path);

    // Подключить внешний индекс к колонке.
    // При `rebuild = true` существующие строки будут добавлены в индекс.
    void attach_index(std::size_t column, IIndexPtr index, bool rebuild = true);

    // Отключить индекс от колонки.
    void detach_index(std::size_t column);

    // Есть ли подключенный индекс у колонки.
    [[nodiscard]] bool has_index(std::size_t column) const;

private:
    struct IndexBinding {
        IIndexPtr index;
    };

    TableSchema schema_;
    std::vector<std::vector<Value>> rows_;
    std::unordered_map<std::size_t, IndexBinding> indexes_;

    void notify_indexes_insert(std::size_t row_id, const std::vector<Value>& row);
};

} 