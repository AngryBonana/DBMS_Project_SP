#pragma once
#include <cstddef>
#include <vector>
#include <optional>
#include <stdexcept>
#include "sql/schema.h"

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

private:
    TableSchema schema_;
    std::vector<std::vector<Value>> rows_;
};

} 