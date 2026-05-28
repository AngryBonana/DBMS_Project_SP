/**
 * @file query_engine.h
 * @brief SQL-исполнитель для подзадач 6 и 7 (DML/SELECT) поверх storage.
 */
#pragma once

#include <filesystem>
#include <mutex>
#include <string>

#include "core/dbms.h"
#include "parser.h"

namespace executor {

/**
 * Исполняет SQL-подобные команды, используя существующие lexer/parser/storage.
 * Поддерживает:
 * - CREATE/DROP/USE DATABASE, CREATE/DROP TABLE
 * - INSERT/UPDATE/DELETE + WHERE
 * - SELECT (* и col AS alias) + JSON-массив
 */
class DbmsQueryEngine {
public:
    explicit DbmsQueryEngine(std::filesystem::path dataRoot);

    /// Выполнить одну команду (с ';' или без) и вернуть строковый результат.
    std::string execute(const std::string& query);

    /// Для тестов оптимизатора: использовался ли индекс на последнем запросе.
    bool lastSelectUsedIndex() const noexcept { return lastSelectUsedIndex_; }

private:
    std::string execute_create_database(const CreateDatabaseCmd& cmd);
    std::string execute_drop_database(const DropDatabaseCmd& cmd);
    std::string execute_use_database(const UseDatabaseCmd& cmd);
    std::string execute_create_table(const CreateTableCmd& cmd);
    std::string execute_drop_table(const DropTableCmd& cmd);
    std::string execute_insert(const InsertCmd& cmd);
    std::string execute_update(const UpdateCmd& cmd);
    std::string execute_delete(const DeleteCmd& cmd);
    std::string execute_select(const SelectCmd& cmd);

    cw_db::Database& resolve_database(const std::string& explicitDbName);
    cw_db::Table& resolve_table(const std::string& explicitDbName, const std::string& tableName);

    cw_db::Value parser_value_to_storage(const ::Value& v) const;
    cw_db::DataType parser_type_to_storage(ColumnDef::Type t) const;
    cw_db::TableSchema build_schema(const std::vector<ColumnDef>& columns) const;

    bool evaluate_condition(const Condition& condition,
                            const cw_db::Table& table,
                            const std::vector<cw_db::Value>& row) const;
    bool evaluate_simple_condition(const Condition& condition,
                                   const cw_db::Table& table,
                                   const std::vector<cw_db::Value>& row) const;
    cw_db::Value resolve_operand(const std::string& text, bool isColumn,
                                 const cw_db::Table& table,
                                 const std::vector<cw_db::Value>& row) const;
    cw_db::Value parse_literal_for_column(const std::string& text,
                                          cw_db::DataType expectedType) const;

    std::vector<cw_db::RowId> collect_candidate_rows(const cw_db::Table& table,
                                                     const std::optional<Condition>& where,
                                                     bool& usedIndex) const;
    std::string value_to_json(const cw_db::Value& value) const;
    std::string row_to_json_object(const cw_db::Table& table,
                                   const std::vector<cw_db::Value>& row,
                                   const SelectCmd& cmd) const;
    std::string escape_json(const std::string& s) const;

    std::shared_ptr<cw_db::IIndex> make_index_for_column(const std::string& dbName,
                                                          const std::string& tableName,
                                                          const cw_db::ColumnDef& column) const;
    void attach_indexes_for_table(const std::string& dbName,
                                  const std::string& tableName,
                                  cw_db::Table& table);
    void attach_indexes_for_loaded_data();

    std::filesystem::path dataRoot_;
    mutable std::mutex mutex_;
    cw_db::Dbms dbms_;
    bool lastSelectUsedIndex_ = false;
};

}  // namespace executor
