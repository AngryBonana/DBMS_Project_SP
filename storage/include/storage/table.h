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

struct Row {
    std::vector<Value> values;
};

// Класс таблицы: хранит схему и строки с устойчивыми RowId.
class Table {
public:
    Table() = default;
    explicit Table(const TableSchema& schema) : schema_(schema) {}
    Table(std::string name, const TableSchema& schema) : name_(std::move(name)), schema_(schema) {}

    // Вставляет одну строку значений с возможными пропусками (std::nullopt)
    // Значения соответствуют порядку колонок в схеме
    void insert_row(const std::vector<std::optional<Value>>& values);

    // Вставляет полностью материализованную строку и возвращает её RowId.
    RowId insert(Row row);

    // Обновляет одно значение в строке.
    void update(RowId id, std::size_t column, const Value& new_value);

    // Удаляет строку по RowId.
    void erase(RowId id);

    // Материализует полную строку из частичного INSERT.
    Row materialize(const std::vector<int>& column_positions, std::vector<Value> values) const;

    // Возвращает индекс колонки или бросает исключение.
    int require_column(const std::string& name) const;

    // Получить материализованную строку
    const std::vector<Value>& get_row(std::size_t idx) const {
        return rows_.at(order_.at(idx)).values;
    }

    std::size_t row_count() const noexcept { return order_.size(); }

    const std::unordered_map<RowId, Row>& rows() const noexcept { return rows_; }

    const std::vector<RowId>& order() const noexcept { return order_; }

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

    // Непривязанный доступ к индексу колонки; nullptr если индекс не подключён.
    [[nodiscard]] IIndex* index_for(std::size_t column) const;

private:
    struct IndexBinding {
        IIndexPtr index;
    };

    std::string name_;
    TableSchema schema_;
    std::unordered_map<RowId, Row> rows_;
    std::vector<RowId> order_;
    std::unordered_map<std::size_t, IndexBinding> indexes_;
    RowId next_id_ = 1;

    void validate(const Row& row, std::optional<RowId> existing_id = std::nullopt) const;
    bool value_exists_in_column(std::size_t column, const Value& value,
                                std::optional<RowId> exclude_id) const;
    void notify_indexes_insert(RowId row_id, const Row& row);
    void notify_indexes_update(RowId row_id, std::size_t column, const Value& old_value, const Value& new_value);
    void notify_indexes_erase(const Row& row);
};

} 