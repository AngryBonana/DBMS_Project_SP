#ifndef PARSER_H
#define PARSER_H
#include "lexer.h"
#include <memory>
#include <optional>
#include <variant>
#include <vector>
#include <string>


using Value = std::variant<int, std::string, std::monostate>; // monostate = NULL


struct Condition {
    enum class Op { EQ, NEQ, LT, GT, LTE, GTE, BETWEEN, LIKE };

    std::string left; // столбец или константа
    bool leftIsColumn; // true = имя столбца, false = константа
    Op op;
    std::string right;
    bool rightIsColumn;
    std::string right2; // только для BETWEEN
    bool right2IsColumn;
};


struct CreateDatabaseCmd {
    std::string dbName;
};

struct DropDatabaseCmd {
    std::string dbName;
};

struct UseDatabaseCmd {
    std::string dbName;
};

struct ColumnDef {
    enum class Type { INT, STRING };
    enum class Modifier { NONE, NOT_NULL, INDEXED };

    std::string name;
    Type type;
    Modifier modifier;
};

struct CreateTableCmd {
    std::string dbName; // пусто, если используется контекст USE
    std::string tableName;
    std::vector<ColumnDef> columns;
};

struct DropTableCmd {
    std::string dbName;
    std::string tableName;
};

struct InsertCmd {
    std::string dbName;
    std::string tableName;
    std::vector<std::string> columns;
    std::vector<std::vector<Value>> rows;
};

struct Assignment {
    std::string column;
    Value value;
};

struct UpdateCmd {
    std::string dbName;
    std::string tableName;
    std::vector<Assignment> assignments;
    std::optional<Condition> where;
};

struct DeleteCmd {
    std::string dbName;
    std::string tableName;
    std::optional<Condition> where;
};

struct SelectColumn {
    std::string name; // "*" если звёздочка
    std::string alias; // пусто, если AS не указан
};

struct SelectCmd {
    std::string dbName;
    std::string tableName;
    bool star;
    std::vector<SelectColumn> columns;
    std::optional<Condition> where;
};


using Command = std::variant<
    CreateDatabaseCmd,
    DropDatabaseCmd,
    UseDatabaseCmd,
    CreateTableCmd,
    DropTableCmd,
    InsertCmd,
    UpdateCmd,
    DeleteCmd,
    SelectCmd
>;


class ParseError : public std::exception {
public:
    explicit ParseError(const std::string& msg) : msg_(msg) {}
    const char* what() const noexcept override { return msg_.c_str(); }
private:
    std::string msg_;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    Command parse();

private:
    std::vector<Token> _tokens;
    size_t _pos;

    Token& peek();
    Token& advance();
    bool check(TokenType type) const;
    bool isEnd() const;

    // Если текущий токен совпадает — съедает и возвращает true
    bool match(TokenType type);

    // Съедает токен нужного типа или бросает ParseError
    Token expect(TokenType type, const std::string& context);

    // Читает "db.table" или просто "table", записывает в dbName/tableName
    void parseTableRef(std::string& dbName, std::string& tableName);

    // Читает значение: INT_LITERAL, STR_LITERAL или NULL
    Value parseValue();

    // Читает одну сторону условия — колонку или константу
    // isColumn выставляется в true, если это идентификатор
    std::string parseOperand(bool& isColumn);

    Condition parseCondition();

    Command parseCreate(); // CREATE DATABASE | CREATE TABLE
    Command parseDrop(); // DROP DATABASE | DROP TABLE
    Command parseUse();
    Command parseInsert();
    Command parseUpdate();
    Command parseDelete();
    Command parseSelect();
};

#endif //PARSER_H