#include "../include/parser.h"
#include <stdexcept>
#include <algorithm>

Parser::Parser(std::vector<Token> tokens) 
    : _tokens(std::move(tokens)), _pos(0) {
}

Command Parser::parse() {
    if (isEnd()) {
        throw ParseError("Empty input");
    }

    Token& token = peek();
    
    switch (token.type) {
        case TokenType::KW_CREATE:
            return parseCreate();
        case TokenType::KW_DROP:
            return parseDrop();
        case TokenType::KW_USE:
            return parseUse();
        case TokenType::KW_INSERT:
            return parseInsert();
        case TokenType::KW_UPDATE:
            return parseUpdate();
        case TokenType::KW_DELETE:
            return parseDelete();
        case TokenType::KW_SELECT:
            return parseSelect();
        default:
            throw ParseError("Unexpected token: " + token.value + 
            " at line " + std::to_string(token.line));
    }
}


Token& Parser::peek() {
    if (isEnd()) {
        throw ParseError("Unexpected end of input");
    }
    return _tokens[_pos];
}

Token& Parser::advance() {
    if (isEnd()) {
        throw ParseError("Unexpected end of input");
    }
    return _tokens[_pos++];
}

bool Parser::check(TokenType type) const {
    return !isEnd() && _tokens[_pos].type == type;
}

bool Parser::isEnd() const {
    return _pos >= _tokens.size() || _tokens[_pos].type == TokenType::END;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        _pos++;
        return true;
    }
    return false;
}

Token Parser::expect(TokenType type, const std::string& context) {
    if (check(type)) {
        return advance();
    }
    Token current = peek();
    throw ParseError("Expected " + context + " at line " + 
                    std::to_string(current.line) + ", got '" + current.value + "'");
}

void Parser::parseTableRef(std::string& dbName, std::string& tableName) {
    Token first = expect(TokenType::IDENTIFIER, "table name");
    
    if (match(TokenType::DOT)) {
        dbName = first.value;
        Token second = expect(TokenType::IDENTIFIER, "table name after dot");
        tableName = second.value;
    } else {
        dbName = "";
        tableName = first.value;
    }
}

Value Parser::parseValue() {
    if (match(TokenType::KW_NULL)) {
        return std::monostate{};
    }
    
    if (check(TokenType::INT_LITERAL)) {
        Token token = advance();
        return std::stoi(token.value);
    }
    
    if (check(TokenType::STR_LITERAL)) {
        Token token = advance();
        return token.value;
    }
    
    throw ParseError("Expected value at line " + std::to_string(peek().line));
}

std::string Parser::parseOperand(bool& isColumn) {
    Token token = peek();
    
    if (token.type == TokenType::IDENTIFIER) {
        isColumn = true;
        return advance().value;
    } else if (token.type == TokenType::INT_LITERAL || 
               token.type == TokenType::STR_LITERAL) {
        isColumn = false;
        return advance().value;
    } else if (token.type == TokenType::KW_NULL) {
        isColumn = false;
        advance();
        return "NULL";
    } else {
        throw ParseError("Expected operand at line " + 
                        std::to_string(token.line) + ", got '" + token.value + "'");
    }
}

Condition Parser::parseCondition() {
    return parseOrCondition();
}

Condition Parser::parseOrCondition() {
    Condition cond = parseAndCondition();
    
    while (match(TokenType::KW_OR)) {
        Condition right = parseAndCondition();
        cond = combineConditions(cond, right, Condition::LogicalOp::OR);
    }
    
    return cond;
}

Condition Parser::parseAndCondition() {
    Condition cond = parsePrimaryCondition();
    
    while (match(TokenType::KW_AND)) {
        Condition right = parsePrimaryCondition();
        cond = combineConditions(cond, right, Condition::LogicalOp::AND);
    }
    
    return cond;
}

Condition Parser::parsePrimaryCondition() {
    if (match(TokenType::LPAREN)) {
        Condition cond = parseCondition();
        expect(TokenType::RPAREN, "')'");
        return cond;
    }
    
    Condition cond;
    cond.left = parseOperand(cond.leftIsColumn);
    
    Token op = peek();
    switch (op.type) {
        case TokenType::OP_EQ:
            cond.op = Condition::Op::EQ;
            break;
        case TokenType::OP_NEQ:
            cond.op = Condition::Op::NEQ;
            break;
        case TokenType::OP_LT:
            cond.op = Condition::Op::LT;
            break;
        case TokenType::OP_GT:
            cond.op = Condition::Op::GT;
            break;
        case TokenType::OP_LTE:
            cond.op = Condition::Op::LTE;
            break;
        case TokenType::OP_GTE:
            cond.op = Condition::Op::GTE;
            break;
        case TokenType::KW_BETWEEN:
            cond.op = Condition::Op::BETWEEN;
            break;
        case TokenType::KW_LIKE:
            cond.op = Condition::Op::LIKE;
            break;
        default:
            throw ParseError("Expected comparison operator at line " + 
                           std::to_string(op.line) + ", got '" + op.value + "'");
    }
    advance();
    
    if (cond.op == Condition::Op::BETWEEN) {
        cond.right = parseOperand(cond.rightIsColumn);
        expect(TokenType::KW_AND, "AND in BETWEEN");
        cond.right2 = parseOperand(cond.right2IsColumn);
    } else {
        cond.right = parseOperand(cond.rightIsColumn);
    }
    
    return cond;
}

Condition Parser::combineConditions(Condition& left, Condition& right, Condition::LogicalOp op) {
    left.logicalOp = op;
    left.next = std::make_unique<Condition>(std::move(right));
    return std::move(left);
}


Command Parser::parseCreate() {
    expect(TokenType::KW_CREATE, "CREATE");
    
    if (match(TokenType::KW_DATABASE)) {
        CreateDatabaseCmd cmd;
        Token name = expect(TokenType::IDENTIFIER, "database name");
        cmd.dbName = name.value;
        return cmd;
    }
    
    if (match(TokenType::KW_TABLE)) {
        CreateTableCmd cmd;
        
        // Имя таблицы (возможно с базой данных)
        parseTableRef(cmd.dbName, cmd.tableName);
        
        expect(TokenType::LPAREN, "'('");
        
        // Список колонок
        do {
            ColumnDef col;
            Token colName = expect(TokenType::IDENTIFIER, "column name");
            col.name = colName.value;
            
            // Тип колонки
            Token typeToken = expect(TokenType::IDENTIFIER, "column type");
            std::string typeStr = typeToken.value;
            std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::toupper);
            
            if (typeStr == "INT" || typeStr == "INTEGER") {
                col.type = ColumnDef::Type::INT;
            } else if (typeStr == "STR" || typeStr == "STRING" || typeStr == "TEXT") {
                col.type = ColumnDef::Type::STRING;
            } else {
                throw ParseError("Unknown column type: " + typeStr + 
                               " at line " + std::to_string(typeToken.line));
            }
            
            // Модификаторы
            col.modifier = ColumnDef::Modifier::NONE;
            
            if (match(TokenType::KW_NOT_NULL)) {
                col.modifier = ColumnDef::Modifier::NOT_NULL;
            } else if (match(TokenType::KW_INDEXED)) {
                col.modifier = ColumnDef::Modifier::INDEXED;
            }
            
            // Проверяем комбинацию NOT NULL INDEXED
            if (col.modifier == ColumnDef::Modifier::NOT_NULL && 
                match(TokenType::KW_INDEXED)) {
                col.modifier = ColumnDef::Modifier::INDEXED;
            } else if (col.modifier == ColumnDef::Modifier::INDEXED && 
                      match(TokenType::KW_NOT_NULL)) {
            }
            
            cmd.columns.push_back(col);
            
        } while (match(TokenType::COMMA));
        
        expect(TokenType::RPAREN, "')'");
        
        return cmd;
    }
    
    if (match(TokenType::KW_TABLE)) {
        CreateTableCmd cmd;
        parseTableRef(cmd.dbName, cmd.tableName);
        expect(TokenType::LPAREN, "'('");
        
        do {
            ColumnDef col;
            Token colName = expect(TokenType::IDENTIFIER, "column name");
            col.name = colName.value;
            
            // Тип колонки
            Token typeToken = expect(TokenType::IDENTIFIER, "column type");
            std::string typeStr = typeToken.value;
            std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::toupper);
            
            if (typeStr == "INT" || typeStr == "INTEGER") {
                col.type = ColumnDef::Type::INT;
            } else if (typeStr == "STR" || typeStr == "STRING" || typeStr == "TEXT") {
                col.type = ColumnDef::Type::STRING;
            } else {
                throw ParseError("Unknown column type: " + typeStr + 
                               " at line " + std::to_string(typeToken.line));
            }
            
            col.modifier = ColumnDef::Modifier::NONE;
            
            bool hasNotNull = false;
            bool hasIndexed = false;
            
            while (true) {
                if (match(TokenType::KW_NOT_NULL)) {
                    hasNotNull = true;
                } else if (match(TokenType::KW_INDEXED)) {
                    hasIndexed = true;
                } else {
                    break;
                }
            }
            
            if (hasNotNull && hasIndexed) {
                col.modifier = ColumnDef::Modifier::INDEXED;
            } else if (hasNotNull) {
                col.modifier = ColumnDef::Modifier::NOT_NULL;
            } else if (hasIndexed) {
                col.modifier = ColumnDef::Modifier::INDEXED;
            }
            
            if (match(TokenType::KW_DEFAULT)) {
                col.defaultValue = parseValue();
            }
            
            cmd.columns.push_back(col);
            
        } while (match(TokenType::COMMA));
        
        expect(TokenType::RPAREN, "')'");
        return cmd;
    }
    
    throw ParseError("Expected DATABASE or TABLE after CREATE at line " + 
                    std::to_string(peek().line));
}

Command Parser::parseDrop() {
    expect(TokenType::KW_DROP, "DROP");
    
    if (match(TokenType::KW_DATABASE)) {
        DropDatabaseCmd cmd;
        Token name = expect(TokenType::IDENTIFIER, "database name");
        cmd.dbName = name.value;
        return cmd;
    }
    
    if (match(TokenType::KW_TABLE)) {
        DropTableCmd cmd;
        parseTableRef(cmd.dbName, cmd.tableName);
        return cmd;
    }
    
    throw ParseError("Expected DATABASE or TABLE after DROP at line " + 
                    std::to_string(peek().line));
}

Command Parser::parseUse() {
    expect(TokenType::KW_USE, "USE");
    
    UseDatabaseCmd cmd;
    Token name = expect(TokenType::IDENTIFIER, "database name");
    cmd.dbName = name.value;
    
    return cmd;
}

Command Parser::parseInsert() {
    InsertCmd cmd;
    
    expect(TokenType::KW_INSERT, "INSERT");
    expect(TokenType::KW_INTO, "INTO");
    
    parseTableRef(cmd.dbName, cmd.tableName);
    
    // Список колонок (опциональный)
    if (match(TokenType::LPAREN)) {
        do {
            Token col = expect(TokenType::IDENTIFIER, "column name");
            cmd.columns.push_back(col.value);
        } while (match(TokenType::COMMA));
        
        expect(TokenType::RPAREN, "')'");
    }
    
    expect(TokenType::KW_VALUE, "VALUE or VALUES");
    
    // Список значений (может быть несколько строк)
    do {
        expect(TokenType::LPAREN, "'('");
        std::vector<std::optional<Value>> row;
        
        bool firstValue = true;
        do {
            if (!firstValue) {
            }
            firstValue = false;
            
            if (check(TokenType::COMMA) || check(TokenType::RPAREN)) {
                row.push_back(std::nullopt);  
            } else {
                row.push_back(parseValue()); 
            }
        } while (match(TokenType::COMMA));
        
        expect(TokenType::RPAREN, "')'");
        cmd.rows.push_back(row);
        
    } while (match(TokenType::COMMA));
    
    return cmd;
}

Command Parser::parseUpdate() {
    UpdateCmd cmd;
    
    expect(TokenType::KW_UPDATE, "UPDATE");
    
    parseTableRef(cmd.dbName, cmd.tableName);
    
    expect(TokenType::KW_SET, "SET");
    
    // Список присваиваний
    do {
        Assignment assign;
        Token col = expect(TokenType::IDENTIFIER, "column name");
        assign.column = col.value;
        
        expect(TokenType::OP_ASSIGN, "'='");
        
        assign.value = parseValue();
        
        cmd.assignments.push_back(assign);
    } while (match(TokenType::COMMA));
    
    // Опциональный WHERE
    if (match(TokenType::KW_WHERE)) {
        cmd.where = parseCondition();
    }
    
    return cmd;
}

Command Parser::parseDelete() {
    DeleteCmd cmd;
    
    expect(TokenType::KW_DELETE, "DELETE");
    expect(TokenType::KW_FROM, "FROM");
    
    parseTableRef(cmd.dbName, cmd.tableName);
    
    // Опциональный WHERE
    if (match(TokenType::KW_WHERE)) {
        cmd.where = parseCondition();
    }
    
    return cmd;
}

Command Parser::parseSelect() {
    SelectCmd cmd;
    expect(TokenType::KW_SELECT, "SELECT");
    
    if (match(TokenType::STAR)) {
        cmd.star = true;
    } else {
        cmd.star = false;
        
        do {
            SelectColumn col;
            
            // Проверяем агрегатные функции
            if (match(TokenType::KW_COUNT)) {
                col.aggregateFunc = AggregateFunc::COUNT;
                expect(TokenType::LPAREN, "'('");
                if (match(TokenType::STAR)) {
                    col.name = "*";
                } else {
                    Token token = expect(TokenType::IDENTIFIER, "column name");
                    col.name = token.value;
                }
                expect(TokenType::RPAREN, "')'");
            } else if (match(TokenType::KW_SUM)) {
                col.aggregateFunc = AggregateFunc::SUM;
                expect(TokenType::LPAREN, "'('");
                Token token = expect(TokenType::IDENTIFIER, "column name");
                col.name = token.value;
                expect(TokenType::RPAREN, "')'");
            } else if (match(TokenType::KW_AVG)) {
                col.aggregateFunc = AggregateFunc::AVG;
                expect(TokenType::LPAREN, "'('");
                Token token = expect(TokenType::IDENTIFIER, "column name");
                col.name = token.value;
                expect(TokenType::RPAREN, "')'");
            } else {
                // Обычная колонка
                Token token = expect(TokenType::IDENTIFIER, "column name");
                col.name = token.value;
                col.aggregateFunc = AggregateFunc::NONE;
            }
            
            // Опциональный алиас
            if (match(TokenType::KW_AS)) {
                Token alias = expect(TokenType::IDENTIFIER, "alias name");
                col.alias = alias.value;
            }
            
            cmd.columns.push_back(col);
        } while (match(TokenType::COMMA));
    }
    
    expect(TokenType::KW_FROM, "FROM");
    parseTableRef(cmd.dbName, cmd.tableName);
    
    if (match(TokenType::KW_WHERE)) {
        cmd.where = parseCondition();
    }
    
    return cmd;
}