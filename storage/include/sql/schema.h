#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/value.h"

namespace cw_db {

// Описание колонки таблицы
struct ColumnDef {
    // Имя колонки
    std::string name;
    // Тип данных столбца
    DataType type = DataType::Int;
    // Флаги: обязательность и наличие индекса
    bool not_null = false;
    bool indexed = false;
    // Значение по умолчанию (если есть)
    std::optional<Value> default_value;
};

// Схема таблицы: упорядоченный набор колонок + быстрый поиск по имени
class TableSchema {
public:
    TableSchema() = default;

    explicit TableSchema(std::vector<ColumnDef> columns) {
        set_columns(std::move(columns));
    }

    // Полный список столбцов в порядке объявления
    // Список колонок в порядке объявления
    const std::vector<ColumnDef>& columns() const noexcept { return columns_; }

    // Количество столбцов
    // Количество колонок
    std::size_t column_count() const noexcept { return columns_.size(); }

    // Пустая схема
    // Пустая схема
    bool empty() const noexcept { return columns_.empty(); }

    // Есть ли столбец с таким именем
    // Есть ли колонка с таким именем
    bool has_column(const std::string& name) const noexcept {
        return index_by_name_.find(name) != index_by_name_.end();
    }

    // Попытка найти индекс столбца
    // Найти индекс колонки по имени (или nullopt)
    std::optional<std::size_t> find_index(const std::string& name) const noexcept {
        auto it = index_by_name_.find(name);
        if (it == index_by_name_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    // Индекс столбца или исключение
    // Получить индекс или бросить исключение при отсутствии
    std::size_t index_of(const std::string& name) const {
        auto index = find_index(name);
        if (!index.has_value()) {
            throw std::out_of_range("TableSchema: unknown column '" + name + "'");
        }
        return *index;
    }

    // Столбец по индексу или исключение
    // Доступ по индексу
    const ColumnDef& at(std::size_t index) const {
        return columns_.at(index);
    }

    // Столбец по имени или исключение
    // Доступ по имени (через индекс_of)
    const ColumnDef& column(const std::string& name) const {
        return at(index_of(name));
    }

private:
    std::vector<ColumnDef> columns_;
    std::unordered_map<std::string, std::size_t> index_by_name_;

    // Установить список колонок (валидирует имена и уникальность)
    void set_columns(std::vector<ColumnDef> columns) {
        columns_.clear();
        index_by_name_.clear();

        columns_.reserve(columns.size());
        for (std::size_t i = 0; i < columns.size(); ++i) {
            const auto& column = columns[i];
            if (column.name.empty()) {
                throw std::invalid_argument("TableSchema: column name must not be empty");
            }
            if (index_by_name_.find(column.name) != index_by_name_.end()) {
                throw std::invalid_argument("TableSchema: duplicate column '" + column.name + "'");
            }

            index_by_name_.emplace(column.name, i);
            columns_.push_back(std::move(columns[i]));
        }
    }
};

}