/**
 * @file request_api.cpp
 * @brief Реализация парсера служебных команд executor.
 */
#include "request_api.h"
#include "string_utils.h"

#include <cctype>
#include <vector>

namespace executor {

namespace {

std::string removeOptionalSemicolon(std::string s) {
    if (!s.empty() && s.back() == ';') {
        s.pop_back();
    }
    return trim(s);
}

std::vector<std::string> splitBySpace(const std::string& text) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

}  // namespace

ApiCommand parseApiCommand(const std::string& rawCommand) {
    const std::string normalized = removeOptionalSemicolon(trim(rawCommand));
    const std::vector<std::string> parts = splitBySpace(normalized);

    if (parts.size() == 3 && toUpperAscii(parts[0]) == "GET") {
        const std::string operation = toUpperAscii(parts[1]);
        if (operation == "STATUS") {
            return ApiCommand{ApiCommandKind::GetStatus, parts[2]};
        }
        if (operation == "RESULT") {
            return ApiCommand{ApiCommandKind::GetResult, parts[2]};
        }
    }

    return ApiCommand{ApiCommandKind::SubmitQuery, rawCommand};
}

}  // namespace executor
