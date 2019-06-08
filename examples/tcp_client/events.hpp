#pragma once

#include <asio.hpp>

#include <system_error>


class retry {};
class terminated {
public:
    terminated() = default;
    terminated(const std::error_code& ec) noexcept: ec(ec) {}

    operator std::error_code() const {
        return ec;
    }

    std::error_code ec;
};

class failed {
public:
    failed() = default;
    failed(const std::error_code& ec) noexcept: ec(ec) {}
    operator std::error_code() const {
        return ec;
    }    
    std::error_code ec;
};

class resolved {
public:
    resolved(asio::ip::tcp::endpoint ep) : ep(ep) {}
    operator asio::ip::tcp::endpoint() const {
        return ep;
    }
    asio::ip::tcp::endpoint ep;
};

class connected {
public:
    connected(asio::ip::tcp::socket& sock) noexcept: sock(sock) {}
    std::reference_wrapper<asio::ip::tcp::socket> sock;

    operator asio::ip::tcp::socket&() const {
        return sock.get();
    }
};