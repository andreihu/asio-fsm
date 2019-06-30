#pragma once

#include <system_error>

struct tick {};
struct tock {};

class terminated {
public:
    terminated() = default;
    terminated(const std::error_code& ec) noexcept: ec(ec) {}

    operator std::error_code() const {
        return ec;
    }

    std::error_code ec;
};


