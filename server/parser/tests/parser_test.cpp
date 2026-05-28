#include <gtest/gtest.h>
#include "../include/parser.h"
#include "../include/lexer.h"
#include <variant>

class ParserTest : public ::testing::Test {
protected:
    Command parse(const std::string& input) {
        Lexer lexer(input);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse();
    }
    
    template<typename T>
    T& getCommand(Command& cmd) {
        return std::get<T>(cmd);
    }
};

// ── Тесты CREATE ─────────────────────────────────────────────────────────────

TEST_F(ParserTest, CreateDatabase) {
    auto cmd = parse("CREATE DATABASE mydb");
    
    ASSERT_TRUE(std::holds_alternative<CreateDatabaseCmd>(cmd));
    auto& createCmd = std::get<CreateDatabaseCmd>(cmd);
    EXPECT_EQ(createCmd.dbName, "mydb");
}

TEST_F(ParserTest, CreateTableBasic) {
    auto cmd = parse("CREATE TABLE users (id INT, name STR)");
    
    ASSERT_TRUE(std::holds_alternative<CreateTableCmd>(cmd));
    auto& createCmd = std::get<CreateTableCmd>(cmd);
    
    EXPECT_EQ(createCmd.tableName, "users");
    EXPECT_EQ(createCmd.dbName, "");
    ASSERT_EQ(createCmd.columns.size(), 2);
    
    EXPECT_EQ(createCmd.columns[0].name, "id");
    EXPECT_EQ(createCmd.columns[0].type, ColumnDef::Type::INT);
    EXPECT_EQ(createCmd.columns[0].modifier, ColumnDef::Modifier::NONE);
    
    EXPECT_EQ(createCmd.columns[1].name, "name");
    EXPECT_EQ(createCmd.columns[1].type, ColumnDef::Type::STRING);
    EXPECT_EQ(createCmd.columns[1].modifier, ColumnDef::Modifier::NONE);
}

TEST_F(ParserTest, CreateTableWithModifiers) {
    auto cmd = parse("CREATE TABLE products (id INT NOT_NULL INDEXED, price INT NOT_NULL)");
    
    ASSERT_TRUE(std::holds_alternative<CreateTableCmd>(cmd));
    auto& createCmd = std::get<CreateTableCmd>(cmd);
    
    ASSERT_EQ(createCmd.columns.size(), 2);
    
    EXPECT_EQ(createCmd.columns[0].name, "id");
    EXPECT_EQ(createCmd.columns[0].modifier, ColumnDef::Modifier::INDEXED);
    
    EXPECT_EQ(createCmd.columns[1].name, "price");
    EXPECT_EQ(createCmd.columns[1].modifier, ColumnDef::Modifier::NOT_NULL);
}

TEST_F(ParserTest, CreateTableWithDefaultValues) {
    auto cmd = parse("CREATE TABLE users (id INT DEFAULT 0, name STR DEFAULT 'Unknown', active INT DEFAULT 1)");
    
    ASSERT_TRUE(std::holds_alternative<CreateTableCmd>(cmd));
    auto& createCmd = std::get<CreateTableCmd>(cmd);
    
    ASSERT_EQ(createCmd.columns.size(), 3);
    
    EXPECT_TRUE(createCmd.columns[0].defaultValue.has_value());
    EXPECT_EQ(std::get<int>(createCmd.columns[0].defaultValue.value()), 0);
    
    EXPECT_TRUE(createCmd.columns[1].defaultValue.has_value());
    EXPECT_EQ(std::get<std::string>(createCmd.columns[1].defaultValue.value()), "Unknown");
    
    EXPECT_TRUE(createCmd.columns[2].defaultValue.has_value());
    EXPECT_EQ(std::get<int>(createCmd.columns[2].defaultValue.value()), 1);
}

TEST_F(ParserTest, CreateTableWithDatabase) {
    auto cmd = parse("CREATE TABLE mydb.users (id INT)");
    
    ASSERT_TRUE(std::holds_alternative<CreateTableCmd>(cmd));
    auto& createCmd = std::get<CreateTableCmd>(cmd);
    
    EXPECT_EQ(createCmd.dbName, "mydb");
    EXPECT_EQ(createCmd.tableName, "users");
}

TEST_F(ParserTest, CreateTableWithDefaultNull) {
    auto cmd = parse("CREATE TABLE test (col1 INT DEFAULT NULL)");
    
    ASSERT_TRUE(std::holds_alternative<CreateTableCmd>(cmd));
    auto& createCmd = std::get<CreateTableCmd>(cmd);
    
    ASSERT_EQ(createCmd.columns.size(), 1);
    EXPECT_TRUE(createCmd.columns[0].defaultValue.has_value());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(createCmd.columns[0].defaultValue.value()));
}

// ── Тесты DROP ──────────────────────────────────────────────────────────────

TEST_F(ParserTest, DropDatabase) {
    auto cmd = parse("DROP DATABASE testdb");
    
    ASSERT_TRUE(std::holds_alternative<DropDatabaseCmd>(cmd));
    auto& dropCmd = std::get<DropDatabaseCmd>(cmd);
    EXPECT_EQ(dropCmd.dbName, "testdb");
}

TEST_F(ParserTest, DropTable) {
    auto cmd = parse("DROP TABLE users");
    
    ASSERT_TRUE(std::holds_alternative<DropTableCmd>(cmd));
    auto& dropCmd = std::get<DropTableCmd>(cmd);
    EXPECT_EQ(dropCmd.tableName, "users");
    EXPECT_EQ(dropCmd.dbName, "");
}

TEST_F(ParserTest, DropTableWithDatabase) {
    auto cmd = parse("DROP TABLE mydb.users");
    
    ASSERT_TRUE(std::holds_alternative<DropTableCmd>(cmd));
    auto& dropCmd = std::get<DropTableCmd>(cmd);
    EXPECT_EQ(dropCmd.dbName, "mydb");
    EXPECT_EQ(dropCmd.tableName, "users");
}

// ── Тесты USE ───────────────────────────────────────────────────────────────

TEST_F(ParserTest, UseDatabase) {
    auto cmd = parse("USE mydb");
    
    ASSERT_TRUE(std::holds_alternative<UseDatabaseCmd>(cmd));
    auto& useCmd = std::get<UseDatabaseCmd>(cmd);
    EXPECT_EQ(useCmd.dbName, "mydb");
}

// ── Тесты INSERT ────────────────────────────────────────────────────────────

TEST_F(ParserTest, InsertBasicValues) {
    auto cmd = parse("INSERT INTO users (name, age) VALUES ('John', 25)");
    
    ASSERT_TRUE(std::holds_alternative<InsertCmd>(cmd));
    auto& insertCmd = std::get<InsertCmd>(cmd);
    
    EXPECT_EQ(insertCmd.tableName, "users");
    ASSERT_EQ(insertCmd.columns.size(), 2);
    EXPECT_EQ(insertCmd.columns[0], "name");
    EXPECT_EQ(insertCmd.columns[1], "age");
    
    ASSERT_EQ(insertCmd.rows.size(), 1);
    ASSERT_EQ(insertCmd.rows[0].size(), 2);
    EXPECT_EQ(std::get<std::string>(insertCmd.rows[0][0].value()), "John");
    EXPECT_EQ(std::get<int>(insertCmd.rows[0][1].value()), 25);
}

TEST_F(ParserTest, InsertMultipleRows) {
    auto cmd = parse("INSERT INTO users VALUES ('Alice', 30), ('Bob', 25), ('Charlie', 35)");
    
    ASSERT_TRUE(std::holds_alternative<InsertCmd>(cmd));
    auto& insertCmd = std::get<InsertCmd>(cmd);
    
    EXPECT_EQ(insertCmd.tableName, "users");
    EXPECT_EQ(insertCmd.columns.size(), 0);
    ASSERT_EQ(insertCmd.rows.size(), 3);
    
    EXPECT_EQ(std::get<std::string>(insertCmd.rows[0][0].value()), "Alice");
    EXPECT_EQ(std::get<int>(insertCmd.rows[0][1].value()), 30);
    
    EXPECT_EQ(std::get<std::string>(insertCmd.rows[2][0].value()), "Charlie");
    EXPECT_EQ(std::get<int>(insertCmd.rows[2][1].value()), 35);
}

TEST_F(ParserTest, InsertWithNull) {
    auto cmd = parse("INSERT INTO users (name, age) VALUES (NULL, 25)");
    
    ASSERT_TRUE(std::holds_alternative<InsertCmd>(cmd));
    auto& insertCmd = std::get<InsertCmd>(cmd);
    
    ASSERT_EQ(insertCmd.rows.size(), 1);
    ASSERT_EQ(insertCmd.rows[0].size(), 2);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(insertCmd.rows[0][0].value()));
    EXPECT_EQ(std::get<int>(insertCmd.rows[0][1].value()), 25);
}

TEST_F(ParserTest, InsertWithDatabase) {
    auto cmd = parse("INSERT INTO mydb.users (name) VALUES ('Test')");
    
    ASSERT_TRUE(std::holds_alternative<InsertCmd>(cmd));
    auto& insertCmd = std::get<InsertCmd>(cmd);
    
    EXPECT_EQ(insertCmd.dbName, "mydb");
    EXPECT_EQ(insertCmd.tableName, "users");
}

// ── Тесты UPDATE ────────────────────────────────────────────────────────────

TEST_F(ParserTest, UpdateBasic) {
    auto cmd = parse("UPDATE users SET name = 'John', age = 30 WHERE id == 1");
    
    ASSERT_TRUE(std::holds_alternative<UpdateCmd>(cmd));
    auto& updateCmd = std::get<UpdateCmd>(cmd);
    
    EXPECT_EQ(updateCmd.tableName, "users");
    ASSERT_EQ(updateCmd.assignments.size(), 2);
    
    EXPECT_EQ(updateCmd.assignments[0].column, "name");
    EXPECT_EQ(std::get<std::string>(updateCmd.assignments[0].value), "John");
    
    EXPECT_EQ(updateCmd.assignments[1].column, "age");
    EXPECT_EQ(std::get<int>(updateCmd.assignments[1].value), 30);
    
    ASSERT_TRUE(updateCmd.where.has_value());
    EXPECT_EQ(updateCmd.where->left, "id");
    EXPECT_TRUE(updateCmd.where->leftIsColumn);
    EXPECT_EQ(updateCmd.where->op, Condition::Op::EQ);
    EXPECT_EQ(updateCmd.where->right, "1");
}

TEST_F(ParserTest, UpdateWithoutWhere) {
    auto cmd = parse("UPDATE users SET status = 'inactive'");
    
    ASSERT_TRUE(std::holds_alternative<UpdateCmd>(cmd));
    auto& updateCmd = std::get<UpdateCmd>(cmd);
    
    EXPECT_EQ(updateCmd.tableName, "users");
    ASSERT_EQ(updateCmd.assignments.size(), 1);
    EXPECT_FALSE(updateCmd.where.has_value());
}

// ── Тесты DELETE ────────────────────────────────────────────────────────────

TEST_F(ParserTest, DeleteBasic) {
    auto cmd = parse("DELETE FROM users WHERE id == 5");
    
    ASSERT_TRUE(std::holds_alternative<DeleteCmd>(cmd));
    auto& deleteCmd = std::get<DeleteCmd>(cmd);
    
    EXPECT_EQ(deleteCmd.tableName, "users");
    ASSERT_TRUE(deleteCmd.where.has_value());
    EXPECT_EQ(deleteCmd.where->op, Condition::Op::EQ);
}

TEST_F(ParserTest, DeleteWithoutWhere) {
    auto cmd = parse("DELETE FROM users");
    
    ASSERT_TRUE(std::holds_alternative<DeleteCmd>(cmd));
    auto& deleteCmd = std::get<DeleteCmd>(cmd);
    
    EXPECT_EQ(deleteCmd.tableName, "users");
    EXPECT_FALSE(deleteCmd.where.has_value());
}

// ── Тесты SELECT ────────────────────────────────────────────────────────────

TEST_F(ParserTest, SelectStar) {
    auto cmd = parse("SELECT * FROM users");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    EXPECT_EQ(selectCmd.tableName, "users");
    EXPECT_TRUE(selectCmd.star);
    EXPECT_EQ(selectCmd.columns.size(), 0);
}

TEST_F(ParserTest, SelectSpecificColumns) {
    auto cmd = parse("SELECT name, age FROM users");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    EXPECT_FALSE(selectCmd.star);
    ASSERT_EQ(selectCmd.columns.size(), 2);
    EXPECT_EQ(selectCmd.columns[0].name, "name");
    EXPECT_EQ(selectCmd.columns[1].name, "age");
}

TEST_F(ParserTest, SelectWithAlias) {
    auto cmd = parse("SELECT name AS username, age AS user_age FROM users");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    ASSERT_EQ(selectCmd.columns.size(), 2);
    EXPECT_EQ(selectCmd.columns[0].name, "name");
    EXPECT_EQ(selectCmd.columns[0].alias, "username");
    EXPECT_EQ(selectCmd.columns[1].name, "age");
    EXPECT_EQ(selectCmd.columns[1].alias, "user_age");
}

// ── Тесты агрегатных функций ─────────────────────────────────────────────────

TEST_F(ParserTest, SelectSumFunction) {
    auto cmd = parse("SELECT SUM(price) FROM products");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    ASSERT_EQ(selectCmd.columns.size(), 1);
    EXPECT_EQ(selectCmd.columns[0].aggregateFunc, AggregateFunc::SUM);
    EXPECT_EQ(selectCmd.columns[0].name, "price");
}

TEST_F(ParserTest, SelectCountStar) {
    auto cmd = parse("SELECT COUNT(*) FROM users");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    ASSERT_EQ(selectCmd.columns.size(), 1);
    EXPECT_EQ(selectCmd.columns[0].aggregateFunc, AggregateFunc::COUNT);
}

TEST_F(ParserTest, SelectAvgWithAlias) {
    auto cmd = parse("SELECT AVG(rating) AS avg_rating FROM reviews");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    ASSERT_EQ(selectCmd.columns.size(), 1);
    EXPECT_EQ(selectCmd.columns[0].aggregateFunc, AggregateFunc::AVG);
    EXPECT_EQ(selectCmd.columns[0].alias, "avg_rating");
}

TEST_F(ParserTest, SelectMultipleAggregates) {
    auto cmd = parse("SELECT COUNT(*) AS total, SUM(amount) AS sum, AVG(price) AS avg_price FROM orders");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    ASSERT_EQ(selectCmd.columns.size(), 3);
    EXPECT_EQ(selectCmd.columns[0].aggregateFunc, AggregateFunc::COUNT);
    EXPECT_EQ(selectCmd.columns[1].aggregateFunc, AggregateFunc::SUM);
    EXPECT_EQ(selectCmd.columns[2].aggregateFunc, AggregateFunc::AVG);
}

// ── Тесты простых условий WHERE ──────────────────────────────────────────────

TEST_F(ParserTest, WhereEquals) {
    auto cmd = parse("SELECT * FROM users WHERE id == 1");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    EXPECT_EQ(selectCmd.where->op, Condition::Op::EQ);
    EXPECT_EQ(selectCmd.where->left, "id");
    EXPECT_EQ(selectCmd.where->right, "1");
}

TEST_F(ParserTest, WhereGreaterThan) {
    auto cmd = parse("SELECT * FROM users WHERE age > 18");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    EXPECT_EQ(selectCmd.where->op, Condition::Op::GT);
}

TEST_F(ParserTest, WhereBetween) {
    auto cmd = parse("SELECT * FROM products WHERE price BETWEEN 10 AND 100");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    EXPECT_EQ(selectCmd.where->op, Condition::Op::BETWEEN);
    EXPECT_EQ(selectCmd.where->left, "price");
    EXPECT_EQ(selectCmd.where->right, "10");
    EXPECT_EQ(selectCmd.where->right2, "100");
}

TEST_F(ParserTest, WhereLike) {
    auto cmd = parse("SELECT * FROM users WHERE name LIKE 'John%'");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    EXPECT_EQ(selectCmd.where->op, Condition::Op::LIKE);
}

TEST_F(ParserTest, WhereNullComparison) {
    auto cmd = parse("SELECT * FROM users WHERE email == NULL");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    EXPECT_EQ(selectCmd.where->right, "NULL");
}

// ── Тесты составных условий WHERE ────────────────────────────────────────────

TEST_F(ParserTest, WhereSimpleAnd) {
    auto cmd = parse("SELECT * FROM users WHERE age > 18 AND status == 'active'");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    
    const Condition& cond = selectCmd.where.value();
    EXPECT_EQ(cond.op, Condition::Op::GT);
    EXPECT_EQ(cond.logicalOp, Condition::LogicalOp::AND);
    ASSERT_NE(cond.next, nullptr);
    EXPECT_EQ(cond.next->op, Condition::Op::EQ);
}

TEST_F(ParserTest, WhereSimpleOr) {
    auto cmd = parse("SELECT * FROM users WHERE age < 18 OR age > 65");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    
    const Condition& cond = selectCmd.where.value();
    EXPECT_EQ(cond.logicalOp, Condition::LogicalOp::OR);
    ASSERT_NE(cond.next, nullptr);
}

TEST_F(ParserTest, WhereMultipleAnds) {
    auto cmd = parse("SELECT * FROM users WHERE age > 18 AND status == 'active' AND score >= 100");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    
    const Condition* cond = &selectCmd.where.value();
    ASSERT_EQ(cond->logicalOp, Condition::LogicalOp::AND);
    ASSERT_NE(cond->next, nullptr);
    
    cond = cond->next.get();
    ASSERT_EQ(cond->logicalOp, Condition::LogicalOp::AND);
    ASSERT_NE(cond->next, nullptr);
    EXPECT_EQ(cond->next->op, Condition::Op::GTE);
}

TEST_F(ParserTest, WhereAndOrPriority) {
    auto cmd = parse("SELECT * FROM users WHERE age > 18 AND status == 'active' OR role == 'admin'");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    
    // Должен быть: (age > 18 AND status == 'active') OR role == 'admin'
    const Condition& cond = selectCmd.where.value();
    EXPECT_EQ(cond.logicalOp, Condition::LogicalOp::OR);
    ASSERT_NE(cond.next, nullptr);
    EXPECT_EQ(cond.next->op, Condition::Op::EQ);
    EXPECT_EQ(cond.next->logicalOp, Condition::LogicalOp::NONE);
}

TEST_F(ParserTest, WhereParentheses) {
    auto cmd = parse("SELECT * FROM users WHERE (age > 18 OR role == 'admin') AND status == 'active'");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    
    const Condition& cond = selectCmd.where.value();
    // Верхний уровень: (...) AND status == 'active'
    EXPECT_EQ(cond.logicalOp, Condition::LogicalOp::AND);
    
    // Правая часть: status == 'active'
    ASSERT_NE(cond.next, nullptr);
    EXPECT_EQ(cond.next->op, Condition::Op::EQ);
    EXPECT_EQ(cond.next->left, "status");
    EXPECT_EQ(cond.next->right, "active");
    
    // Левая часть в leftTree: (age > 18 OR role == 'admin')
    ASSERT_NE(cond.leftTree, nullptr);
    EXPECT_EQ(cond.leftTree->logicalOp, Condition::LogicalOp::OR);
    EXPECT_EQ(cond.leftTree->op, Condition::Op::GT);
    ASSERT_NE(cond.leftTree->next, nullptr);
    EXPECT_EQ(cond.leftTree->next->op, Condition::Op::EQ);
}

TEST_F(ParserTest, WhereNestedParentheses) {
    auto cmd = parse("SELECT * FROM users WHERE ((age > 18 AND age < 65) OR role == 'admin') AND status == 'active'");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    
    // Структура должна быть корректной
    const Condition& cond = selectCmd.where.value();
    EXPECT_EQ(cond.logicalOp, Condition::LogicalOp::AND);
    ASSERT_NE(cond.next, nullptr);
    EXPECT_EQ(cond.next->op, Condition::Op::EQ);
}

TEST_F(ParserTest, WhereComplexWithBetween) {
    auto cmd = parse("SELECT * FROM products WHERE price BETWEEN 10 AND 100 AND category == 'electronics'");
    
    auto& selectCmd = std::get<SelectCmd>(cmd);
    ASSERT_TRUE(selectCmd.where.has_value());
    
    const Condition& cond = selectCmd.where.value();
    EXPECT_EQ(cond.op, Condition::Op::BETWEEN);
    EXPECT_EQ(cond.logicalOp, Condition::LogicalOp::AND);
    ASSERT_NE(cond.next, nullptr);
    EXPECT_EQ(cond.next->op, Condition::Op::EQ);
}

// ── Тесты комбинированных запросов ───────────────────────────────────────────

TEST_F(ParserTest, SelectWithEverything) {
    auto cmd = parse(
        "SELECT name, SUM(amount) AS total_amount, AVG(price) "
        "FROM mydb.orders "
        "WHERE status == 'completed' AND amount > 100 OR customer_type == 'vip'"
    );
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    EXPECT_EQ(selectCmd.dbName, "mydb");
    EXPECT_EQ(selectCmd.tableName, "orders");
    EXPECT_FALSE(selectCmd.star);
    ASSERT_EQ(selectCmd.columns.size(), 3);
    
    EXPECT_EQ(selectCmd.columns[0].name, "name");
    EXPECT_EQ(selectCmd.columns[0].aggregateFunc, AggregateFunc::NONE);
    
    EXPECT_EQ(selectCmd.columns[1].name, "amount");
    EXPECT_EQ(selectCmd.columns[1].aggregateFunc, AggregateFunc::SUM);
    EXPECT_EQ(selectCmd.columns[1].alias, "total_amount");
    
    EXPECT_EQ(selectCmd.columns[2].name, "price");
    EXPECT_EQ(selectCmd.columns[2].aggregateFunc, AggregateFunc::AVG);
    EXPECT_EQ(selectCmd.columns[2].alias, "");
    
    ASSERT_TRUE(selectCmd.where.has_value());
    EXPECT_EQ(selectCmd.where->logicalOp, Condition::LogicalOp::OR);
}

// ── Тесты ошибок парсинга ───────────────────────────────────────────────────

TEST_F(ParserTest, ParseErrorEmptyInput) {
    EXPECT_THROW(parse(""), ParseError);
}

TEST_F(ParserTest, ParseErrorUnexpectedToken) {
    EXPECT_THROW(parse("123 SELECT * FROM users"), ParseError);
}

TEST_F(ParserTest, ParseErrorMissingKeyword) {
    EXPECT_THROW(parse("SELECT * users"), ParseError);
}

TEST_F(ParserTest, ParseErrorMissingParenthesis) {
    EXPECT_THROW(parse("CREATE TABLE test (id INT"), ParseError);
}

TEST_F(ParserTest, ParseErrorInvalidOperator) {
    EXPECT_THROW(parse("SELECT * FROM users WHERE id = 1"), ParseError);
}

TEST_F(ParserTest, ParseErrorIncompleteBetween) {
    EXPECT_THROW(parse("SELECT * FROM users WHERE age BETWEEN 10"), ParseError);
}

TEST_F(ParserTest, ParseErrorInvalidAggregateFunction) {
    EXPECT_THROW(parse("SELECT UNKNOWN(price) FROM products"), ParseError);
}

TEST_F(ParserTest, ParseErrorUnclosedString) {
    EXPECT_THROW(parse("SELECT * FROM users WHERE name == 'unclosed"), ParseError);
}

// ── Тесты на разные стили написания ─────────────────────────────────────────

TEST_F(ParserTest, CaseInsensitiveKeywords) {
    auto cmd = parse("select * from users where id == 1");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    EXPECT_EQ(selectCmd.tableName, "users");
    EXPECT_TRUE(selectCmd.star);
    ASSERT_TRUE(selectCmd.where.has_value());
}

TEST_F(ParserTest, MixedCaseKeywords) {
    auto cmd = parse("SeLeCt * FrOm users WhErE id == 1");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
}

TEST_F(ParserTest, ExtraWhitespace) {
    auto cmd = parse("  SELECT   *    FROM    users   WHERE   id   ==   1   ");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    EXPECT_EQ(selectCmd.tableName, "users");
    EXPECT_TRUE(selectCmd.star);
}

// ── Тесты VALUES с несколькими формами ──────────────────────────────────────

TEST_F(ParserTest, InsertValuesKeywordVariants) {
    auto cmd1 = parse("INSERT INTO users VALUES (1, 'test')");
    auto cmd2 = parse("INSERT INTO users VALUE (2, 'test2')");
    
    ASSERT_TRUE(std::holds_alternative<InsertCmd>(cmd1));
    ASSERT_TRUE(std::holds_alternative<InsertCmd>(cmd2));
    
    auto& insert1 = std::get<InsertCmd>(cmd1);
    auto& insert2 = std::get<InsertCmd>(cmd2);
    
    EXPECT_EQ(insert1.rows.size(), 1);
    EXPECT_EQ(insert2.rows.size(), 1);
}

// ── Тесты на граничные случаи ───────────────────────────────────────────────

TEST_F(ParserTest, TableNameWithUnderscores) {
    auto cmd = parse("SELECT * FROM my_table_name_123");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    EXPECT_EQ(selectCmd.tableName, "my_table_name_123");
}

TEST_F(ParserTest, ColumnNamesLikeKeywords) {
    auto cmd = parse("SELECT `select`, `from`, `where` FROM keywords_table");
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    // Имена колонок, похожие на ключевые слова, должны распознаваться как идентификаторы
}

TEST_F(ParserTest, MultipleWhereConditionsWithBetween) {
    auto cmd = parse(
        "SELECT * FROM orders "
        "WHERE amount BETWEEN 100 AND 1000 "
        "AND status == 'completed' "
        "OR priority == 'high'"
    );
    
    ASSERT_TRUE(std::holds_alternative<SelectCmd>(cmd));
    auto& selectCmd = std::get<SelectCmd>(cmd);
    
    ASSERT_TRUE(selectCmd.where.has_value());
    
    // Верхний уровень: (... AND ...) OR priority == 'high'
    const Condition& cond = selectCmd.where.value();
    EXPECT_EQ(cond.logicalOp, Condition::LogicalOp::OR);
    
    // Правая часть: priority == 'high'
    ASSERT_NE(cond.next, nullptr);
    EXPECT_EQ(cond.next->op, Condition::Op::EQ);
    EXPECT_EQ(cond.next->left, "priority");
    
    // Левая часть в leftTree: amount BETWEEN 100 AND 1000 AND status == 'completed'
    ASSERT_NE(cond.leftTree, nullptr);
    EXPECT_EQ(cond.leftTree->op, Condition::Op::BETWEEN);
    EXPECT_EQ(cond.leftTree->logicalOp, Condition::LogicalOp::AND);
    ASSERT_NE(cond.leftTree->next, nullptr);
    EXPECT_EQ(cond.leftTree->next->op, Condition::Op::EQ);
}

// ── Тесты на полный цикл всех команд ────────────────────────────────────────

TEST_F(ParserTest, FullCycleOfCommands) {
    // Проверяем, что все команды парсятся без ошибок
    
    EXPECT_NO_THROW(parse("CREATE DATABASE testdb"));
    EXPECT_NO_THROW(parse("USE testdb"));
    EXPECT_NO_THROW(parse("CREATE TABLE users (id INT NOT_NULL INDEXED, name STR DEFAULT 'Unknown', age INT)"));
    EXPECT_NO_THROW(parse("INSERT INTO users (name, age) VALUES ('Alice', 25)"));
    EXPECT_NO_THROW(parse("INSERT INTO users VALUES ('Bob', 30), ('Charlie', 35)"));
    EXPECT_NO_THROW(parse("SELECT * FROM users"));
    EXPECT_NO_THROW(parse("SELECT name, age FROM users WHERE age >= 18 AND status == 'active'"));
    EXPECT_NO_THROW(parse("SELECT COUNT(*) AS total, AVG(age) AS avg_age FROM users"));
    EXPECT_NO_THROW(parse("UPDATE users SET age = 26 WHERE name == 'Alice'"));
    EXPECT_NO_THROW(parse("DELETE FROM users WHERE age < 18 OR age > 65"));
    EXPECT_NO_THROW(parse("DROP TABLE users"));
    EXPECT_NO_THROW(parse("DROP DATABASE testdb"));
}