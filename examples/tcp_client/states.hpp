#pragma once

// ours
#include "events.hpp"

#include <log.hpp>
#include <test_state_base.hpp>

#include <afsm/state.hpp>
#include <afsm/util/type_name.hpp>

// thirdparty
#include <asio.hpp>

// std
#include <cmath>
#include <functional>
#include <string>
#include <system_error>
#include <variant>

class resolving : public test_state_base<resolving, failed, resolved, terminated> {
public:
    resolving(asio::io_service& io, const std::string& addr, const std::string& service) :
        test_state_base(io),
        resolver(io),
        addr(addr),
        service(service)
    {}

    virtual void on_enter() override {
        resolver.async_resolve(addr, service, track([this](const std::error_code& ec, asio::ip::tcp::resolver::iterator it) {
            ec ? complete<failed>(ec) : complete<resolved>(*it);
        }));
    }

    virtual void cancel() override {
        complete<terminated>();
        resolver.cancel();
    }
private:
    asio::ip::tcp::resolver resolver;
    std::string             addr;
    std::string             service;
};

class connecting : public test_state_base<connecting, failed, connected, terminated> {
public:
    connecting(asio::io_service& io, const asio::ip::tcp::endpoint& ep) :
        test_state_base(io),
        sock(io),
        ep(ep)
    {}

    virtual void on_enter() override {
        sock.async_connect(ep, track([this](const std::error_code& ec) {
            ec ? complete<failed>(ec) : complete<connected>(sock);
        }));
    }

    virtual void cancel() override {
        complete<terminated>();
        sock.cancel();
    }
private:
    asio::ip::tcp::socket   sock;
    asio::ip::tcp::endpoint ep;
};

class online : public test_state_base<online, failed, terminated> {
public:
    online(asio::io_service& io, asio::ip::tcp::socket& sock) :
        test_state_base(io),
        sock(std::move(sock)),
        timer(io),
        extend(false)
    {}

    virtual void on_enter() override {
        start_read_socket();
        start_wait_timer();
    }

    virtual void cancel() override {
        complete<terminated>();
        sock.cancel();
        timer.cancel();
    }

private:
    void start_read_socket() {
        using namespace std::literals;
        asio::async_read_until(sock, asio::dynamic_buffer(rx_buffer), "\n"sv, track([this] (const std::error_code& ec, size_t bytes_transferred) {
            if (ec) {
                return complete<failed>(ec);
            }

            log("got {}", rx_buffer);
            rx_buffer.clear();
            start_read_socket();
            extend = true;
            start_wait_timer();
        }));
    }

    void start_wait_timer() {
        timer.expires_from_now(std::chrono::seconds(10));
        timer.async_wait(track([this](const std::error_code& ec) {
            if (ec) {
                if (ec == asio::error::operation_aborted && extend) {
                    extend = false;
                    return;
                }

                return complete<failed>(ec);
            }

            return complete<failed>(make_error_code(std::errc::timed_out));
        }));
    }
private:
    asio::ip::tcp::socket   sock;
    asio::steady_timer      timer;
    bool                    extend;
    std::string             rx_buffer;
};

class backoff : public test_state_base<backoff, retry, failed, terminated> {
public:
    backoff(asio::io_service& io, std::chrono::seconds cooldown) :
        test_state_base(io),
        timer(io)
    {
        timer.expires_from_now(cooldown);
    }

    virtual void on_enter() override {
        timer.async_wait(track([this](const std::error_code& ec) {
            ec ? complete<failed>(ec) : complete<retry>();
        }));
    }

    virtual void cancel() override {
        timer.cancel();
    }
private:
    asio::steady_timer timer;
};

struct completed {
    completed(asio::io_service& io, const std::error_code& ec) {}
};
