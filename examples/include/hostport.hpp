#pragma once

#include <string>
#include <optional>
#include <cstidnt>

struct hostport {
    std::string             host;
    std::optional<uint16_t> port;

    explicit hostport(const std::string& host, std::optional<uint16_t> port = std::nullopt) : host(host), port(port) {}
};