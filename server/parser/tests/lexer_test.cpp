#include <gtest/gtest.h>
#include "../include/lexer.h"
#include <vector>

class LexerTest : public ::testing::Test {
protected:
    std::vector<Token> tokenize(const std::string& input) {
        Lexer lexer(input);
        return lexer.tokenize();
    }
    
    void expectToken(const Token& token, TokenType type, const std::string& value, int line) {
        EXPECT_EQ(token.type, type) << "Token type mismatch";
        EXPECT_EQ(token.value, value) << "Token value mismatch";
        EXPECT_EQ(token.line, line) << "Token line mismatch";
    }
};

// ── Тесты базовых токенов ────────────────────────────────────────────────────

TEST_F(LexerTest, EmptyInput) {
    auto tokens = tokenize("");
    ASSERT_EQ(tokens.size(), 1);
    expectToken(tokens[0], TokenType::END, "", 1);
}

TEST_F(LexerTest, Keywords) {
    auto tokens = tokenize("SELECT FROM WHERE AS INSERT INTO VALUE UPDATE SET DELETE CREATE DROP USE DATABASE TABLE NOT_NULL INDEXED DEFAULT BETWEEN AND OR LIKE SUM COUNT AVG");
    
    struct ExpectedToken {
        TokenType type;
        std::string value;
    };
    
    std::vector<ExpectedToken> expected = {
        {TokenType::KW_SELECT, "SELECT"},
        {TokenType::KW_FROM, "FROM"},
        {TokenType::KW_WHERE, "WHERE"},
        {TokenType::KW_AS, "AS"},
        {TokenType::KW_INSERT, "INSERT"},
        {TokenType::KW_INTO, "INTO"},
        {TokenType::KW_VALUE, "VALUE"},
        {TokenType::KW_UPDATE, "UPDATE"},
        {TokenType::KW_SET, "SET"},
        {TokenType::KW_DELETE, "DELETE"},
        {TokenType::KW_CREATE, "CREATE"},
        {TokenType::KW_DROP, "DROP"},
        {TokenType::KW_USE, "USE"},
        {TokenType::KW_DATABASE, "DATABASE"},
        {TokenType::KW_TABLE, "TABLE"},
        {TokenType::KW_NOT_NULL, "NOT_NULL"},
        {TokenType::KW_INDEXED, "INDEXED"},
        {TokenType::KW_DEFAULT, "DEFAULT"},
        {TokenType::KW_BETWEEN, "BETWEEN"},
        {TokenType::KW_AND, "AND"},
        {TokenType::KW_OR, "OR"},
        {TokenType::KW_LIKE, "LIKE"},
        {TokenType::KW_SUM, "SUM"},
        {TokenType::KW_COUNT, "COUNT"},
        {TokenType::KW_AVG, "AVG"},
    };
    
    ASSERT_EQ(tokens.size(), expected.size() + 1); // +1 для END
    
    for (size_t i = 0; i < expected.size(); ++i) {
        expectToken(tokens[i], expected[i].type, expected[i].value, 1);
    }
}

TEST_F(LexerTest, CaseInsensitiveKeywords) {
    auto tokens = tokenize("select Select SELECT");
    
    ASSERT_EQ(tokens.size(), 4); // 3 SELECT + END
    expectToken(tokens[0], TokenType::KW_SELECT, "select", 1);
    expectToken(tokens[1], TokenType::KW_SELECT, "Select", 1);
    expectToken(tokens[2], TokenType::KW_SELECT, "SELECT", 1);
}

// ── Тесты идентификаторов ────────────────────────────────────────────────────

TEST_F(LexerTest, Identifiers) {
    auto tokens = tokenize("table_name _private var123 test");
    
    ASSERT_EQ(tokens.size(), 5); // 4 IDENTIFIER + END
    
    expectToken(tokens[0], TokenType::IDENTIFIER, "table_name", 1);
    expectToken(tokens[1], TokenType::IDENTIFIER, "_private", 1);
    expectToken(tokens[2], TokenType::IDENTIFIER, "var123", 1);
    expectToken(tokens[3], TokenType::IDENTIFIER, "test", 1);
}

TEST_F(LexerTest, MixedIdentifiersAndKeywords) {
    auto tokens = tokenize("SELECT my_table FROM my_database");
    
    ASSERT_EQ(tokens.size(), 5); // 4 токена + END
    
    expectToken(tokens[0], TokenType::KW_SELECT, "SELECT", 1);
    expectToken(tokens[1], TokenType::IDENTIFIER, "my_table", 1);
    expectToken(tokens[2], TokenType::KW_FROM, "FROM", 1);
    expectToken(tokens[3], TokenType::IDENTIFIER, "my_database", 1);
}

// ── Тесты литералов ──────────────────────────────────────────────────────────

TEST_F(LexerTest, IntegerLiterals) {
    auto tokens = tokenize("123 0 456789");
    
    ASSERT_EQ(tokens.size(), 4); // 3 INT + END
    
    expectToken(tokens[0], TokenType::INT_LITERAL, "123", 1);
    expectToken(tokens[1], TokenType::INT_LITERAL, "0", 1);
    expectToken(tokens[2], TokenType::INT_LITERAL, "456789", 1);
}

TEST_F(LexerTest, StringLiterals) {
    auto tokens = tokenize("'hello world' \"test\" 'it\\'s'");
    
    ASSERT_EQ(tokens.size(), 4); // 3 STR + END
    
    expectToken(tokens[0], TokenType::STR_LITERAL, "hello world", 1);
    expectToken(tokens[1], TokenType::STR_LITERAL, "test", 1);
    expectToken(tokens[2], TokenType::STR_LITERAL, "it's", 1);
}

TEST_F(LexerTest, StringWithEscapeSequences) {
    auto tokens = tokenize("'line1\\nline2\\ttab\\\\'");
    
    ASSERT_EQ(tokens.size(), 2); // 1 STR + END
    expectToken(tokens[0], TokenType::STR_LITERAL, "line1\nline2\ttab\\", 1);
}

TEST_F(LexerTest, NullLiteral) {
    auto tokens = tokenize("NULL null Null");
    
    ASSERT_EQ(tokens.size(), 4);
    expectToken(tokens[0], TokenType::KW_NULL, "NULL", 1);
    expectToken(tokens[1], TokenType::KW_NULL, "null", 1);
    expectToken(tokens[2], TokenType::KW_NULL, "Null", 1);
}

// ── Тесты операторов ─────────────────────────────────────────────────────────

TEST_F(LexerTest, ComparisonOperators) {
    auto tokens = tokenize("== != < > <= >=");
    
    ASSERT_EQ(tokens.size(), 7); // 6 операторов + END
    
    expectToken(tokens[0], TokenType::OP_EQ, "==", 1);
    expectToken(tokens[1], TokenType::OP_NEQ, "!=", 1);
    expectToken(tokens[2], TokenType::OP_LT, "<", 1);
    expectToken(tokens[3], TokenType::OP_GT, ">", 1);
    expectToken(tokens[4], TokenType::OP_LTE, "<=", 1);
    expectToken(tokens[5], TokenType::OP_GTE, ">=", 1);
}

TEST_F(LexerTest, AssignmentOperator) {
    auto tokens = tokenize("=");
    
    ASSERT_EQ(tokens.size(), 2);
    expectToken(tokens[0], TokenType::OP_ASSIGN, "=", 1);
}

TEST_F(LexerTest, MixedOperators) {
    auto tokens = tokenize("= == < <= !=");
    
    ASSERT_EQ(tokens.size(), 6);
    expectToken(tokens[0], TokenType::OP_ASSIGN, "=", 1);
    expectToken(tokens[1], TokenType::OP_EQ, "==", 1);
    expectToken(tokens[2], TokenType::OP_LT, "<", 1);
    expectToken(tokens[3], TokenType::OP_LTE, "<=", 1);
    expectToken(tokens[4], TokenType::OP_NEQ, "!=", 1);
}

// ── Тесты знаков препинания ──────────────────────────────────────────────────

TEST_F(LexerTest, Punctuation) {
    auto tokens = tokenize(", ; ( ) * .");
    
    ASSERT_EQ(tokens.size(), 7); // 6 знаков + END
    
    expectToken(tokens[0], TokenType::COMMA, ",", 1);
    expectToken(tokens[1], TokenType::SEMICOLON, ";", 1);
    expectToken(tokens[2], TokenType::LPAREN, "(", 1);
    expectToken(tokens[3], TokenType::RPAREN, ")", 1);
    expectToken(tokens[4], TokenType::STAR, "*", 1);
    expectToken(tokens[5], TokenType::DOT, ".", 1);
}

// ── Тесты пробелов и переносов строк ─────────────────────────────────────────

TEST_F(LexerTest, WhitespaceHandling) {
    auto tokens = tokenize("SELECT   \t  FROM\nWHERE");
    
    ASSERT_EQ(tokens.size(), 4);
    
    expectToken(tokens[0], TokenType::KW_SELECT, "SELECT", 1);
    expectToken(tokens[1], TokenType::KW_FROM, "FROM", 1);
    expectToken(tokens[2], TokenType::KW_WHERE, "WHERE", 2);
}

TEST_F(LexerTest, LineNumbers) {
    auto tokens = tokenize("line1\nline2\nline3");
    
    ASSERT_EQ(tokens.size(), 4);
    
    expectToken(tokens[0], TokenType::IDENTIFIER, "line1", 1);
    expectToken(tokens[1], TokenType::IDENTIFIER, "line2", 2);
    expectToken(tokens[2], TokenType::IDENTIFIER, "line3", 3);
}

// ── Тесты сложных выражений ──────────────────────────────────────────────────

TEST_F(LexerTest, ComplexQuery) {
    auto tokens = tokenize("SELECT name, age FROM users WHERE age >= 18 AND status == 'active'");
    
    ASSERT_GT(tokens.size(), 10);
    
    expectToken(tokens[0], TokenType::KW_SELECT, "SELECT", 1);
    expectToken(tokens[1], TokenType::IDENTIFIER, "name", 1);
    expectToken(tokens[2], TokenType::COMMA, ",", 1);
    expectToken(tokens[3], TokenType::IDENTIFIER, "age", 1);
    expectToken(tokens[4], TokenType::KW_FROM, "FROM", 1);
    expectToken(tokens[5], TokenType::IDENTIFIER, "users", 1);
    expectToken(tokens[6], TokenType::KW_WHERE, "WHERE", 1);
    expectToken(tokens[7], TokenType::IDENTIFIER, "age", 1);
    expectToken(tokens[8], TokenType::OP_GTE, ">=", 1);
    expectToken(tokens[9], TokenType::INT_LITERAL, "18", 1);
    expectToken(tokens[10], TokenType::KW_AND, "AND", 1);
    expectToken(tokens[11], TokenType::IDENTIFIER, "status", 1);
    expectToken(tokens[12], TokenType::OP_EQ, "==", 1);
}

TEST_F(LexerTest, CreateTableWithDefault) {
    auto tokens = tokenize("CREATE TABLE users (id INT NOT_NULL INDEXED, name STR DEFAULT 'Unknown')");
    
    ASSERT_GT(tokens.size(), 10);
    
    expectToken(tokens[0], TokenType::KW_CREATE, "CREATE", 1);
    expectToken(tokens[1], TokenType::KW_TABLE, "TABLE", 1);
    expectToken(tokens[2], TokenType::IDENTIFIER, "users", 1);
    expectToken(tokens[3], TokenType::LPAREN, "(", 1);
    expectToken(tokens[4], TokenType::IDENTIFIER, "id", 1);
    expectToken(tokens[5], TokenType::IDENTIFIER, "INT", 1);
    expectToken(tokens[6], TokenType::KW_NOT_NULL, "NOT_NULL", 1);
    expectToken(tokens[7], TokenType::KW_INDEXED, "INDEXED", 1);
    expectToken(tokens[8], TokenType::COMMA, ",", 1);
    expectToken(tokens[9], TokenType::IDENTIFIER, "name", 1);
    expectToken(tokens[10], TokenType::IDENTIFIER, "STR", 1);
    expectToken(tokens[11], TokenType::KW_DEFAULT, "DEFAULT", 1);
}

// ── Тесты агрегатных функций ─────────────────────────────────────────────────

TEST_F(LexerTest, AggregateFunctions) {
    auto tokens = tokenize("SUM(price) COUNT(*) AVG(rating)");
    
    ASSERT_EQ(tokens.size(), 13); // 12 токенов + END
    
    expectToken(tokens[0], TokenType::KW_SUM, "SUM", 1);
    expectToken(tokens[1], TokenType::LPAREN, "(", 1);
    expectToken(tokens[2], TokenType::IDENTIFIER, "price", 1);
    expectToken(tokens[3], TokenType::RPAREN, ")", 1);
    
    expectToken(tokens[4], TokenType::KW_COUNT, "COUNT", 1);
    expectToken(tokens[5], TokenType::LPAREN, "(", 1);
    expectToken(tokens[6], TokenType::STAR, "*", 1);
    expectToken(tokens[7], TokenType::RPAREN, ")", 1);
    
    expectToken(tokens[8], TokenType::KW_AVG, "AVG", 1);
    expectToken(tokens[9], TokenType::LPAREN, "(", 1);
    expectToken(tokens[10], TokenType::IDENTIFIER, "rating", 1);
    expectToken(tokens[11], TokenType::RPAREN, ")", 1);
}

// ── Тесты граничных случаев ──────────────────────────────────────────────────

TEST_F(LexerTest, OnlyWhitespace) {
    auto tokens = tokenize("   \n\t   \n  ");
    
    ASSERT_EQ(tokens.size(), 1);
    expectToken(tokens[0], TokenType::END, "", 3);
}

TEST_F(LexerTest, VeryLongIdentifier) {
    std::string longName(1000, 'a');
    auto tokens = tokenize(longName);
    
    ASSERT_EQ(tokens.size(), 2);
    expectToken(tokens[0], TokenType::IDENTIFIER, longName, 1);
}

TEST_F(LexerTest, ConsecutiveOperators) {
    auto tokens = tokenize("=== !==");
    
    ASSERT_EQ(tokens.size(), 5); // ==, =, !=, =, END
    
    expectToken(tokens[0], TokenType::OP_EQ, "==", 1);
    expectToken(tokens[1], TokenType::OP_ASSIGN, "=", 1);
    expectToken(tokens[2], TokenType::OP_NEQ, "!=", 1);
    expectToken(tokens[3], TokenType::OP_ASSIGN, "=", 1);
}