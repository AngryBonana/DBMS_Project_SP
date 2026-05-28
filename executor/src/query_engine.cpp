/**
 * @file query_engine.cpp
 * @brief Реализация SQL-исполнителя для задач 6 и 7.
 */
#include "query_engine.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include "index/bstarplus_adapter.h"
#include "lexer.h"
#include "parser.h"

namespace executor {

using cw_db::DataType;
using cw_db::Database;
using cw_db::RowId;
using cw_db::Table;
using cw_db::Value;

namespace {

bool has_terminal_semicolon(const std::string& s) {
    for (std::size_t i = s.size(); i > 0; --i) {
        const char c = s[i - 1];
        if (std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }
        return c == ';';
    }
    return false;
}

std::string with_semicolon_if_missing(const std::string& query) {
    if (has_terminal_semicolon(query)) {
        return query;
    }
    return query + ";";
}

}  // namespace

DbmsQueryEngine::DbmsQueryEngine(std::filesystem::path dataRoot)
    : dataRoot_(std::move(dataRoot)), dbms_(dataRoot_) {
    std::filesystem::create_directories(dataRoot_);
    dbms_.load_all();
    attach_indexes_for_loaded_data();
}

std::string DbmsQueryEngine::execute(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string source = with_semicolon_if_missing(query);
    auto tokens = Lexer(source).tokenize();
    auto command = Parser(std::move(tokens)).parse();

    // Разбор AST → вызов storage/index; ответ "OK" или JSON для SELECT.
    return std::visit(
        [this](const auto& cmd) -> std::string {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, CreateDatabaseCmd>) return execute_create_database(cmd);
            if constexpr (std::is_same_v<T, DropDatabaseCmd>) return execute_drop_database(cmd);
            if constexpr (std::is_same_v<T, UseDatabaseCmd>) return execute_use_database(cmd);
            if constexpr (std::is_same_v<T, CreateTableCmd>) return execute_create_table(cmd);
            if constexpr (std::is_same_v<T, DropTableCmd>) return execute_drop_table(cmd);
            if constexpr (std::is_same_v<T, InsertCmd>) return execute_insert(cmd);
            if constexpr (std::is_same_v<T, UpdateCmd>) return execute_update(cmd);
            if constexpr (std::is_same_v<T, DeleteCmd>) return execute_delete(cmd);
            if constexpr (std::is_same_v<T, SelectCmd>) return execute_select(cmd);
            throw std::runtime_error("Unsupported command type");
        },
        command);
}

std::string DbmsQueryEngine::execute_create_database(const CreateDatabaseCmd& cmd) {
    dbms_.create_database(cmd.dbName);
    dbms_.save_all();
    return "OK";
}

std::string DbmsQueryEngine::execute_drop_database(const DropDatabaseCmd& cmd) {
    dbms_.drop_database(cmd.dbName);
    dbms_.save_all();
    return "OK";
}

std::string DbmsQueryEngine::execute_use_database(const UseDatabaseCmd& cmd) {
    dbms_.use_database(cmd.dbName);
    return "OK";
}

std::string DbmsQueryEngine::execute_create_table(const CreateTableCmd& cmd) {
    Database& db = resolve_database(cmd.dbName);
    auto schema = build_schema(cmd.columns);
    Table& table = db.create_table(cmd.tableName, schema);
    attach_indexes_for_table(db.name(), cmd.tableName, table);
    dbms_.save_all();
    return "OK";
}

std::string DbmsQueryEngine::execute_drop_table(const DropTableCmd& cmd) {
    Database& db = resolve_database(cmd.dbName);
    db.drop_table(cmd.tableName);
    dbms_.save_all();
    return "OK";
}

std::string DbmsQueryEngine::execute_insert(const InsertCmd& cmd) {
    Table& table = resolve_table(cmd.dbName, cmd.tableName);

    std::vector<int> positions;
    if (!cmd.columns.empty()) {
        positions.reserve(cmd.columns.size());
        for (const auto& name : cmd.columns) {
            positions.push_back(table.require_column(name));
        }
    }

    for (const auto& parserRow : cmd.rows) {
        if (cmd.columns.empty()) {
            std::vector<std::optional<Value>> values;
            values.reserve(parserRow.size());
            for (const auto& v : parserRow) {
                if (v.has_value()) {
                    values.push_back(parser_value_to_storage(*v));
                } else {
                    values.push_back(std::nullopt);
                }
            }
            table.insert_row(values);
            continue;
        }

        std::vector<Value> provided;
        provided.reserve(parserRow.size());
        for (const auto& v : parserRow) {
            provided.push_back(v.has_value() ? parser_value_to_storage(*v) : Value::null());
        }
        auto row = table.materialize(positions, std::move(provided));
        table.insert(std::move(row));
    }

    dbms_.save_all();
    return "OK";
}

std::string DbmsQueryEngine::execute_update(const UpdateCmd& cmd) {
    Table& table = resolve_table(cmd.dbName, cmd.tableName);

    bool usedIndex = false;
    const auto candidates = collect_candidate_rows(table, cmd.where, usedIndex);

    for (const RowId id : candidates) {
        auto it = table.rows().find(id);
        if (it == table.rows().end()) {
            continue;
        }
        const auto& row = it->second.values;
        if (cmd.where && !evaluate_condition(*cmd.where, table, row)) {
            continue;
        }

        for (const auto& assignment : cmd.assignments) {
            const std::size_t col = static_cast<std::size_t>(table.require_column(assignment.column));
            table.update(id, col, parser_value_to_storage(assignment.value));
        }
    }

    dbms_.save_all();
    return "OK";
}

std::string DbmsQueryEngine::execute_delete(const DeleteCmd& cmd) {
    Table& table = resolve_table(cmd.dbName, cmd.tableName);

    bool usedIndex = false;
    const auto candidates = collect_candidate_rows(table, cmd.where, usedIndex);
    std::vector<RowId> toDelete;
    toDelete.reserve(candidates.size());

    for (const RowId id : candidates) {
        auto it = table.rows().find(id);
        if (it == table.rows().end()) {
            continue;
        }
        if (cmd.where && !evaluate_condition(*cmd.where, table, it->second.values)) {
            continue;
        }
        toDelete.push_back(id);
    }

    for (const RowId id : toDelete) {
        table.erase(id);
    }

    dbms_.save_all();
    return "OK";
}

std::string DbmsQueryEngine::execute_select(const SelectCmd& cmd) {
    Table& table = resolve_table(cmd.dbName, cmd.tableName);
    bool usedIndex = false;
    const auto candidates = collect_candidate_rows(table, cmd.where, usedIndex);
    lastSelectUsedIndex_ = usedIndex;

    std::ostringstream out;
    out << "[";
    bool first = true;
    for (const RowId id : candidates) {
        auto it = table.rows().find(id);
        if (it == table.rows().end()) {
            continue;
        }
        if (cmd.where && !evaluate_condition(*cmd.where, table, it->second.values)) {
            continue;
        }
        if (!first) {
            out << ",";
        }
        first = false;
        out << row_to_json_object(table, it->second.values, cmd);
    }
    out << "]";
    return out.str();
}

Database& DbmsQueryEngine::resolve_database(const std::string& explicitDbName) {
    if (!explicitDbName.empty()) {
        return dbms_.require_database(explicitDbName);
    }
    return dbms_.active_db();
}

Table& DbmsQueryEngine::resolve_table(const std::string& explicitDbName, const std::string& tableName) {
    Database& db = resolve_database(explicitDbName);
    return db.require_table(tableName);
}

Value DbmsQueryEngine::parser_value_to_storage(const ::Value& v) const {
    if (std::holds_alternative<std::monostate>(v)) return Value::null();
    if (std::holds_alternative<int>(v)) return Value::of_int(std::get<int>(v));
    return Value::of_str(std::get<std::string>(v));
}

DataType DbmsQueryEngine::parser_type_to_storage(ColumnDef::Type t) const {
    return t == ColumnDef::Type::INT ? DataType::Int : DataType::Str;
}

cw_db::TableSchema DbmsQueryEngine::build_schema(const std::vector<ColumnDef>& columns) const {
    std::vector<cw_db::ColumnDef> out;
    out.reserve(columns.size());
    for (const auto& c : columns) {
        cw_db::ColumnDef dst;
        dst.name = c.name;
        dst.type = parser_type_to_storage(c.type);
        dst.not_null = (c.modifier == ColumnDef::Modifier::NOT_NULL || c.modifier == ColumnDef::Modifier::INDEXED);
        dst.indexed = (c.modifier == ColumnDef::Modifier::INDEXED);
        if (c.defaultValue.has_value()) {
            dst.default_value = parser_value_to_storage(*c.defaultValue);
        }
        out.push_back(std::move(dst));
    }
    return cw_db::TableSchema(std::move(out));
}

bool DbmsQueryEngine::evaluate_condition(const Condition& condition,
                                         const Table& table,
                                         const std::vector<Value>& row) const {
    if (condition.leftTree) {
        const bool left = evaluate_condition(*condition.leftTree, table, row);
        const bool right = condition.next ? evaluate_condition(*condition.next, table, row) : false;
        return condition.logicalOp == Condition::LogicalOp::AND ? (left && right) : (left || right);
    }

    bool current = evaluate_simple_condition(condition, table, row);
    if (!condition.next) {
        return current;
    }

    const bool next = evaluate_condition(*condition.next, table, row);
    if (condition.logicalOp == Condition::LogicalOp::AND) return current && next;
    if (condition.logicalOp == Condition::LogicalOp::OR) return current || next;
    return current;
}

bool DbmsQueryEngine::evaluate_simple_condition(const Condition& condition,
                                                const Table& table,
                                                const std::vector<Value>& row) const {
    const Value left = resolve_operand(condition.left, condition.leftIsColumn, table, row);
    const Value right = resolve_operand(condition.right, condition.rightIsColumn, table, row);

    if (condition.op == Condition::Op::BETWEEN) {
        const Value right2 = resolve_operand(condition.right2, condition.right2IsColumn, table, row);
        bool valid1 = false;
        bool valid2 = false;
        const int geLower = left.compare(right, valid1);
        const int ltUpper = left.compare(right2, valid2);
        return valid1 && valid2 && geLower >= 0 && ltUpper < 0;
    }

    if (condition.op == Condition::Op::LIKE) {
        if (!left.is_str() || !right.is_str()) {
            return false;
        }
        std::regex pattern(right.as_str());
        return std::regex_match(left.as_str(), pattern);
    }

    bool valid = false;
    const int cmp = left.compare(right, valid);
    if (!valid) {
        return false;
    }

    switch (condition.op) {
    case Condition::Op::EQ: return cmp == 0;
    case Condition::Op::NEQ: return cmp != 0;
    case Condition::Op::LT: return cmp < 0;
    case Condition::Op::GT: return cmp > 0;
    case Condition::Op::LTE: return cmp <= 0;
    case Condition::Op::GTE: return cmp >= 0;
    case Condition::Op::BETWEEN:
    case Condition::Op::LIKE:
        return false;
    }
    return false;
}

Value DbmsQueryEngine::resolve_operand(const std::string& text, bool isColumn,
                                       const Table& table,
                                       const std::vector<Value>& row) const {
    if (isColumn) {
        const auto idx = static_cast<std::size_t>(table.require_column(text));
        return row.at(idx);
    }

    if (text == "NULL") {
        return Value::null();
    }

    // Пробуем число, иначе считаем строкой.
    try {
        std::size_t pos = 0;
        const long long n = std::stoll(text, &pos);
        if (pos == text.size()) {
            return Value::of_int(n);
        }
    } catch (...) {
    }
    return Value::of_str(text);
}

Value DbmsQueryEngine::parse_literal_for_column(const std::string& text,
                                                DataType expectedType) const {
    if (text == "NULL") {
        return Value::null();
    }
    if (expectedType == DataType::Int) {
        return Value::of_int(std::stoll(text));
    }
    return Value::of_str(text);
}

std::vector<RowId> DbmsQueryEngine::collect_candidate_rows(const Table& table,
                                                           const std::optional<Condition>& where,
                                                           bool& usedIndex) const {
    usedIndex = false;
    std::vector<RowId> out;

    if (!where.has_value()) {
        out = table.order();
        return out;
    }

    const Condition& c = *where;
    const bool simple = !c.leftTree && !c.next;
    if (simple && c.leftIsColumn && !c.rightIsColumn) {
        const std::size_t col = static_cast<std::size_t>(table.require_column(c.left));
        if (table.has_index(col)) {
            const auto* idx = table.index_for(col);
            const auto& def = table.schema().at(col);
            if (idx && (c.op == Condition::Op::EQ || c.op == Condition::Op::BETWEEN)) {
                if (c.op == Condition::Op::EQ) {
                    const Value key = parse_literal_for_column(c.right, def.type);
                    const auto found = idx->find(key);
                    if (found) out.push_back(*found);
                } else {
                    const Value begin = parse_literal_for_column(c.right, def.type);
                    const Value end = parse_literal_for_column(c.right2, def.type);
                    out = idx->range_search(begin, end);
                }
                usedIndex = true;
                return out;
            }
        }
    }

    out = table.order();
    return out;
}

std::string DbmsQueryEngine::value_to_json(const Value& value) const {
    if (value.is_null()) return "null";
    if (value.is_int()) return std::to_string(value.as_int());
    return "\"" + escape_json(value.as_str()) + "\"";
}

std::string DbmsQueryEngine::row_to_json_object(const Table& table,
                                                const std::vector<Value>& row,
                                                const SelectCmd& cmd) const {
    std::ostringstream out;
    out << "{";
    bool first = true;

    if (cmd.star) {
        const auto& cols = table.schema().columns();
        for (std::size_t i = 0; i < cols.size(); ++i) {
            if (!first) out << ",";
            first = false;
            out << "\"" << escape_json(cols[i].name) << "\":" << value_to_json(row[i]);
        }
        out << "}";
        return out.str();
    }

    for (const auto& col : cmd.columns) {
        const std::size_t idx = table.schema().index_of(col.name);
        const std::string key = col.alias.empty() ? col.name : col.alias;
        if (!first) out << ",";
        first = false;
        out << "\"" << escape_json(key) << "\":" << value_to_json(row[idx]);
    }
    out << "}";
    return out.str();
}

std::string DbmsQueryEngine::escape_json(const std::string& s) const {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
        case '\"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += ch; break;
        }
    }
    return out;
}

std::shared_ptr<cw_db::IIndex> DbmsQueryEngine::make_index_for_column(
    const std::string& dbName, const std::string& tableName, const cw_db::ColumnDef& column) const {
    const auto idxDir = dataRoot_ / dbName / "_indexes";
    std::filesystem::create_directories(idxDir);
    const auto idxPath = idxDir / (tableName + "_" + column.name + ".idx");
    const auto keyKind = (column.type == DataType::Int) ? db::IndexKeyKind::Int64 : db::IndexKeyKind::String;
    return std::make_shared<cw_db::BStarPlusIndexAdapter>(idxPath, keyKind);
}

void DbmsQueryEngine::attach_indexes_for_table(const std::string& dbName,
                                               const std::string& tableName,
                                               Table& table) {
    const auto& cols = table.schema().columns();
    for (std::size_t i = 0; i < cols.size(); ++i) {
        if (!cols[i].indexed) {
            continue;
        }
        if (table.has_index(i)) {
            continue;
        }
        table.attach_index(i, make_index_for_column(dbName, tableName, cols[i]), true);
    }
}

void DbmsQueryEngine::attach_indexes_for_loaded_data() {
    for (const auto& [dbName, dbPtr] : dbms_.databases()) {
        for (const auto& [tableName, tablePtr] : dbPtr->tables()) {
            attach_indexes_for_table(dbName, tableName, *tablePtr);
        }
    }
}

}  // namespace executor
