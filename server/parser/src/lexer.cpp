#include "../include/parse_error.h"
#include "../include/lexer.h"
#include <cctype>
#include <unordered_map>
#include <algorithm>
#include <stdexcept>

Lexer::Lexer(const std::string& input) 
    : input_(input), pos_(0), line_(1) {
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!isEnd()) {
        skipWhitespace();
        
        if (isEnd()) break;
        
        char c = peek();
        
        // Идентификаторы и ключевые слова
        if (std::isalpha(c) || c == '_') {
            tokens.push_back(readWord());
        }
        // Числа
        else if (std::isdigit(c)) {
            tokens.push_back(readNumber());
        }
        // Строки
        else if (c == '\'' || c == '"') {
            tokens.push_back(readString());
        }
        // Операторы и знаки препинания
        else {
            Token token = readOperator();
            if (token.type != TokenType::END) {
                tokens.push_back(token);
            } else {
                // Пропускаем неизвестные символы
                advance();
            }
        }
    }
    
    tokens.push_back({TokenType::END, "", line_});
    return tokens;
}

char Lexer::peek() const {
    if (isEnd()) return '\0';
    return input_[pos_];
}

char Lexer::advance() {
    if (isEnd()) return '\0';
    char c = input_[pos_++];
    if (c == '\n') {
        line_++;
    }
    return c;
}

bool Lexer::isEnd() const {
    return pos_ >= input_.length();
}

void Lexer::skipWhitespace() {
    while (!isEnd() && std::isspace(peek())) {
        advance();
    }
}

Token Lexer::readWord() {
    int startLine = line_;
    std::string word;
    
    while (!isEnd() && (std::isalnum(peek()) || peek() == '_')) {
        word += advance();
    }
    
    TokenType type = toKeyword(word);
    return {type, word, startLine};
}

Token Lexer::readNumber() {
    int startLine = line_;
    std::string number;
    
    while (!isEnd() && std::isdigit(peek())) {
        number += advance();
    }
    
    return {TokenType::INT_LITERAL, number, startLine};
}

Token Lexer::readString() {
    int startLine = line_;
    char quote = advance(); // ' или "
    std::string str;
    
    while (!isEnd() && peek() != quote) {
        if (peek() == '\\') {
            advance(); // пропускаем обратный слеш
            if (!isEnd()) {
                char escaped = advance();
                switch (escaped) {
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case '\\': str += '\\'; break;
                    case '\'': str += '\''; break;
                    case '"': str += '"'; break;
                    default: 
                        str += '\\';
                        str += escaped;
                        break;
                }
            }
        } else if (peek() == '\n') {
            str += advance();
        } else {
            str += advance();
        }
    }
    
    if (isEnd()) {
        throw ParseError("Unclosed string at line " + std::to_string(startLine));
    }
    
    advance();
    
    return {TokenType::STR_LITERAL, str, startLine};
}

Token Lexer::readOperator() {
    int startLine = line_;
    char c = advance();
    
    switch (c) {
        case '=':
            if (!isEnd() && peek() == '=') {
                advance();
                return {TokenType::OP_EQ, "==", startLine};
            }
            return {TokenType::OP_ASSIGN, "=", startLine};
                    
        case '!':
            if (!isEnd() && peek() == '=') {
                advance();
                return {TokenType::OP_NEQ, "!=", startLine};
            }
            break;
            
        case '<':
            if (!isEnd() && peek() == '=') {
                advance();
                return {TokenType::OP_LTE, "<=", startLine};
            }
            return {TokenType::OP_LT, "<", startLine};
            
        case '>':
            if (!isEnd() && peek() == '=') {
                advance();
                return {TokenType::OP_GTE, ">=", startLine};
            }
            return {TokenType::OP_GT, ">", startLine};
            
        case ',':
            return {TokenType::COMMA, ",", startLine};
            
        case ';':
            return {TokenType::SEMICOLON, ";", startLine};
            
        case '(':
            return {TokenType::LPAREN, "(", startLine};
            
        case ')':
            return {TokenType::RPAREN, ")", startLine};
            
        case '*':
            return {TokenType::STAR, "*", startLine};
            
        case '.':
            return {TokenType::DOT, ".", startLine};
        
        case '`':
            return {TokenType::BACKTICK, "`", startLine};

        default:
            break;
    }
    
    return {TokenType::END, "", startLine};
}

TokenType Lexer::toKeyword(const std::string& word) const {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"SELECT", TokenType::KW_SELECT},
        {"FROM", TokenType::KW_FROM},
        {"WHERE", TokenType::KW_WHERE},
        {"AS", TokenType::KW_AS},
        {"INSERT", TokenType::KW_INSERT},
        {"INTO", TokenType::KW_INTO},
        {"VALUE", TokenType::KW_VALUE},
        {"VALUES", TokenType::KW_VALUE},
        {"UPDATE", TokenType::KW_UPDATE},
        {"SET", TokenType::KW_SET},
        {"DELETE", TokenType::KW_DELETE},
        {"CREATE", TokenType::KW_CREATE},
        {"DROP", TokenType::KW_DROP},
        {"USE", TokenType::KW_USE},
        {"DATABASE", TokenType::KW_DATABASE},
        {"TABLE", TokenType::KW_TABLE},
        {"NOT", TokenType::KW_NOT_NULL},
        {"NULL", TokenType::KW_NULL},
        {"INDEXED", TokenType::KW_INDEXED},
        {"DEFAULT", TokenType::KW_DEFAULT},
        {"BETWEEN", TokenType::KW_BETWEEN},
        {"AND", TokenType::KW_AND},
        {"OR", TokenType::KW_OR},
        {"LIKE", TokenType::KW_LIKE},
        {"SUM", TokenType::KW_SUM},
        {"COUNT", TokenType::KW_COUNT},
        {"AVG", TokenType::KW_AVG}
    };
    
    // Приводим к верхнему регистру для регистронезависимого сравнения
    std::string upper = word;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    auto it = keywords.find(upper);
    if (it != keywords.end()) {
        return it->second;
    }
    
    return TokenType::IDENTIFIER;
}