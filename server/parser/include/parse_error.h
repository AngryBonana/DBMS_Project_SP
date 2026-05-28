#ifndef PARSE_ERROR_H
#define PARSE_ERROR_H
#include <stdexcept>


class ParseError : public std::exception {
public:
    explicit ParseError(const std::string& msg) : msg_(msg) {}
    const char* what() const noexcept override { return msg_.c_str(); }
private:
    std::string msg_;
};

#endif //PARSE_ERROR_H