/**
 * @file json_response.cpp
 * @brief Сериализация ответов API исполнителя в JSON.
 */
#include "json_response.h"

#include <cstdio>
#include <sstream>

namespace executor {

namespace {

void appendOptionalTimestamp(std::ostringstream& out,
                             const char* fieldName,
                             const std::optional<std::chrono::system_clock::time_point>& tp) {
    out << ",\"" << fieldName << "\":";
    if (tp) {
        out << "\"" << escapeJsonString(formatTimestamp(*tp)) << "\"";
    } else {
        out << "null";
    }
}

}  // namespace

std::string escapeJsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char ch : value) {
        switch (ch) {
        case '"':  escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                escaped += buf;
            } else {
                escaped += ch;
            }
        }
    }
    return escaped;
}

std::string buildSyncResponse(const std::string& result, int statusCode) {
    std::ostringstream out;
    out << "{\"mode\":\"sync\",\"status_code\":" << statusCode
        << ",\"result\":\"" << escapeJsonString(result) << "\"}";
    return out.str();
}

std::string buildAsyncAcceptedResponse(const RequestId& requestId) {
    std::ostringstream out;
    out << "{\"mode\":\"async\",\"request_id\":\""
        << escapeJsonString(requestId) << "\"}";
    return out.str();
}

std::string buildStatusResponse(const RequestStatusInfo& info) {
    std::ostringstream out;
    out << "{\"request_id\":\"" << escapeJsonString(info.id)
        << "\",\"status\":\"" << toString(info.status)
        << "\",\"submitted_at\":\""
        << escapeJsonString(formatTimestamp(info.submittedAt)) << "\"";
    appendOptionalTimestamp(out, "started_at", info.startedAt);
    appendOptionalTimestamp(out, "finished_at", info.finishedAt);
    if (info.queuePosition) {
        out << ",\"queue_position\":" << *info.queuePosition;
    }
    out << "}";
    return out.str();
}

std::string buildResultResponse(const RequestResultInfo& info) {
    std::ostringstream out;
    out << "{\"request_id\":\"" << escapeJsonString(info.id)
        << "\",\"ready\":" << (info.ready ? "true" : "false")
        << ",\"status\":\"" << toString(info.status) << "\"";

    if (info.ready && info.result) {
        out << ",\"result\":\"" << escapeJsonString(*info.result) << "\"";
    }
    if (info.ready && info.error) {
        out << ",\"error\":\"" << escapeJsonString(*info.error) << "\"";
    }

    out << "}";
    return out.str();
}

std::string buildErrorResponse(const std::string& message, int code) {
    std::ostringstream out;
    out << "{\"error\":\"" << escapeJsonString(message)
        << "\",\"code\":" << code << "}";
    return out.str();
}

}  // namespace executor
