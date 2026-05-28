#pragma once

#include "client_socket.h"

#include <chrono>
#include <optional>
#include <string>
#include <thread>

namespace client_protocol {

inline bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

inline std::optional<std::string> extractRequestId(const std::string& json) {
    const std::string key = "\"request_id\":\"";
    const auto pos = json.find(key);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    const auto start = pos + key.size();
    const auto end = json.find('"', start);
    if (end == std::string::npos || end <= start) {
        return std::nullopt;
    }
    return json.substr(start, end - start);
}

inline bool isAsyncAccepted(const std::string& response) {
    return contains(response, "\"mode\":\"async\"");
}

inline bool isResultReady(const std::string& response) {
    return contains(response, "\"ready\":true");
}

// Отправляет SQL; для async-ответа опрашивает GET RESULT до ready.
inline std::string sendAndWait(Client& client, const std::string& command,
                               int maxPolls = 50,
                               std::chrono::milliseconds pollInterval =
                                   std::chrono::milliseconds(100)) {
    std::string response = client.send(command);
    if (!isAsyncAccepted(response)) {
        return response;
    }

    const auto requestId = extractRequestId(response);
    if (!requestId) {
        return response;
    }

    for (int attempt = 0; attempt < maxPolls; ++attempt) {
        const std::string resultJson =
            client.send("GET RESULT " + *requestId + ";");
        if (isResultReady(resultJson)) {
            return resultJson;
        }
        std::this_thread::sleep_for(pollInterval);
    }

    return response;
}

} // namespace client_protocol
