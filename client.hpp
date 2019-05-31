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
    resolving(asio::io_service& io, const std::string& addr, const std::string& service, completion_handler cb) :
        state(io, std::move(cb)),
        resolver(io)
    {
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
};

template<typename Event>
struct state_factory<resolving, Event, context> {
    template<typename Callback>
    state_handle operator()(const Event&, context& ctx, Callback cb) const {
        return std::make_unique<resolving>(ctx.io, ctx.host, ctx.service, std::move(cb));
    }
};

class connecting : public state<failed, connected, terminated> {
public:
    connecting(asio::io_service& io, const asio::ip::tcp::endpoint& ep, completion_handler cb) :
        state(io, std::move(cb)),
        sock(io)
    {
        sock.async_connect(ep, track([this](const std::error_code& ec) {
            ec ? complete<failed>(ec) : complete<connected>(sock);
        }));
    }

    virtual void cancel() override {
        complete<terminated>();
        sock.cancel();
    }
private:
    asio::ip::tcp::socket sock;
};

class online : public state<failed, terminated> {
public:
    online(asio::io_service& io, asio::ip::tcp::socket& sock, completion_handler cb) :
        state(io, std::move(cb)),
        sock(std::move(sock)),
        timer(io),
        extend(false)
    {
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

class backoff : public state<retry, failed, terminated> {
public:
    backoff(asio::io_service& io, std::chrono::seconds cooldown, completion_handler cb) :
        state(io, std::move(cb)),
        timer(io)
    {
        timer.expires_from_now(cooldown);
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
    template<typename Callback>
    std::unique_ptr<state_base> operator()(const Event&, context& ctx, Callback cb) const {
        std::chrono::seconds timeout(std::min<int>(16, std::pow(2, ctx.nbackoff++)));
        return std::make_unique<backoff>(ctx.io, timeout, std::move(cb));
    }
};

using client = fsm<std::error_code, resolving, terminated, context, transitions<
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
