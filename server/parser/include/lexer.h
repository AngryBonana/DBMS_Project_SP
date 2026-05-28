#ifndef LEXER_H
#define LEXER_H
#include <string>
#include <vector>

enum class TokenType {
    /*Ключевые слова*/
    KW_SELECT, KW_FROM, KW_WHERE, KW_AS,
    KW_INSERT, KW_INTO, KW_VALUE,
    KW_UPDATE, KW_SET,
    KW_DELETE,
    KW_CREATE, KW_DROP, KW_USE,
    KW_DATABASE, KW_TABLE,
    KW_NOT_NULL, KW_INDEXED, KW_DEFAULT,
    KW_BETWEEN, KW_AND, KW_LIKE, KW_OR,
    KW_NULL,
    KW_SUM, KW_COUNT, KW_AVG,

    /*Литералы*/
    INT_LITERAL,
    STR_LITERAL,

    IDENTIFIER,

    /*Операторы*/
    OP_EQ,   // ==
    OP_NEQ,  // !=
    OP_LT,   // <
    OP_GT,   // >
    OP_LTE,  // <=
    OP_GTE,  // >=
    OP_ASSIGN, // = (для SET)

    /*Знаки препинания*/
    COMMA,
    SEMICOLON,
    LPAREN,
    RPAREN,
    STAR,
    DOT,
    BACKTICK,

    END
};

struct Token {
    TokenType type;
    std::string value;
    int line; // для сообщений об ошибках
};

class Lexer {
public:
    explicit Lexer(const std::string& input);

    std::vector<Token> tokenize();

private:
    std::string input_;
    size_t pos_;
    int line_;

    char peek() const;
    char advance();
    bool isEnd() const;

    void skipWhitespace();
    Token readWord();
    Token readNumber();
    Token readString();
    Token readOperator();

    TokenType toKeyword(const std::string& word) const;
};

#endif //LEXER_H