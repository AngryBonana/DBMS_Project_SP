/**
 * @file request_types.cpp
 * @brief Реализация генерации и валидации идентификаторов запросов (UUIDv4).
 *
 * Генерирует случайные идентификаторы в формате UUID версии 4
 * с использованием потокобезопасного генератора случайных чисел.
 * Функция isValidRequestId проверяет строку на соответствие этому формату
 * с помощью регулярного выражения.
 */
#include "request_types.h"

#include <array>
#include <cstdio>
#include <random>
#include <regex>

namespace executor {

namespace {

std::mt19937_64& randomEngine() {
    thread_local std::mt19937_64 engine{std::random_device{}()};
    return engine;
}

}  // namespace

RequestId generateRequestId() {
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

    auto next = [&]() { return dist(randomEngine()); };

    const uint32_t timeLow = next();
    const uint16_t timeMid = static_cast<uint16_t>(next() & 0xFFFFu);
    const uint16_t timeHiAndVersion =
        static_cast<uint16_t>((next() & 0x0FFFu) | 0x4000u);
    const uint16_t clockSeq =
        static_cast<uint16_t>((next() & 0x3FFFu) | 0x8000u);
    const uint32_t nodeLow = next();
    const uint16_t nodeHigh = static_cast<uint16_t>(next() & 0xFFFFu);

    std::array<char, 37> buffer{};
    std::snprintf(
        buffer.data(), buffer.size(),
        "%08x-%04x-%04x-%04x-%04x%08x",
        timeLow, timeMid, timeHiAndVersion, clockSeq, nodeHigh, nodeLow);

    return RequestId(buffer.data());
}

bool isValidRequestId(const std::string& id) {
    static const std::regex pattern(
        "^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-"
        "[0-9a-f]{12}$",
        std::regex::ECMAScript);

    return std::regex_match(id, pattern);
}

}  // namespace executor