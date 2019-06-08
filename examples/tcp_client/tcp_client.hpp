#pragma once

#include "fsm.hpp"

#include <asio.hpp>

#include <optional>
#include <variant>
#include <system_error>
#include <functional>
#include <cmath>

struct context {
    context(asio::io_service& io, const std::string& host, const std::string& service) : io(io), host(host), service(service), nbackoff(0) {}
    asio::io_service&       io;
    std::string             host;
    std::string             service;
    size_t                  nbackoff;
};

// events

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

// states

class resolving : public state<failed, resolved, terminated> {
public:
    resolving(asio::io_service& io, const std::string& addr, const std::string& service) :
        state(io),
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

template<typename Event>
struct state_factory<resolving, Event, context> {
    auto operator()(const Event&, context& ctx) const {
        return std::make_tuple(std::ref(ctx.io), ctx.host, ctx.service);
    }
};


class connecting : public state<failed, connected, terminated> {
public:
    connecting(asio::io_service& io, const asio::ip::tcp::endpoint& ep) :
        state(io),
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

class online : public state<failed, terminated> {
public:
    online(asio::io_service& io, asio::ip::tcp::socket& sock) :
        state(io),
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

            log("got %s", rx_buffer);
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

template<typename Event>
struct state_factory<online, Event, context> {
    auto operator()(const Event& ev, context& ctx) const {
        ctx.nbackoff = 0;
        return std::make_tuple(std::ref(ctx.io), ev);
    }
};

class backoff : public state<retry, failed, terminated> {
public:
    backoff(asio::io_service& io, std::chrono::seconds cooldown) :
        state(io),
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

template<typename Event>
struct state_factory<backoff, Event, context> {
    auto operator()(const Event&, context& ctx) const {
        std::chrono::seconds timeout(std::min<int>(16, std::pow(2, ctx.nbackoff++)));
        return std::make_tuple(std::ref(ctx.io), timeout);
    }
};

struct completed {
    completed(asio::io_service& io, const std::error_code& ec) {}
};

using client = fsm<std::error_code, resolving, completed, context, transitions<
    transition<resolving, failed, backoff>,
    transition<resolving, resolved, connecting>,
    transition<resolving, terminated, completed>,

    transition<connecting, failed, backoff>,
    transition<connecting, connected, online>,
    transition<connecting, terminated, completed>,

    transition<online, failed, backoff>,
    transition<online, terminated, completed>,

    transition<backoff, retry, resolving>,
    transition<backoff, failed, completed>,
    transition<backoff, terminated, completed>
>>;
