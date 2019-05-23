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

namespace events {
class retry {};
class shutdown {
public:
    shutdown() = default;
    shutdown(const std::error_code& ec) noexcept: ec(ec) {}
    std::error_code ec;
};

class failure {
public:
    failure() = default;
    failure(const std::error_code& ec) noexcept: ec(ec) {}
    std::error_code ec;
};

} // namespace events

class resolving : public state<events::failure, asio::ip::tcp::endpoint, events::shutdown> {
public:
    resolving(asio::io_service& io, const std::string& addr, const std::string& service, completion_handler cb) :
        state(io, std::move(cb)),
        resolver(io)
    {
        resolver.async_resolve(addr, service, track([this](const std::error_code& ec, asio::ip::tcp::resolver::iterator it) {
            ec ? complete(events::failure(ec)) : complete(*it);
        }));
    }

    virtual void cancel() override {
        complete(events::shutdown{});
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

class connecting : public state<events::failure, std::reference_wrapper<asio::ip::tcp::socket>, events::shutdown> {
public:
    connecting(asio::io_service& io, const asio::ip::tcp::endpoint& ep, completion_handler cb) :
        state(io, std::move(cb)),
        sock(io)
    {
        sock.async_connect(ep, track([this](const std::error_code& ec) {
            ec ? complete(events::failure(ec)) : complete(std::ref(sock));
        }));
    }

    virtual void cancel() override {
        complete(events::shutdown{});
        sock.cancel();
    }
private:
    asio::ip::tcp::socket sock;
};

class online : public state<events::failure, events::shutdown> {
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
        complete(events::shutdown{});
        sock.cancel();
        timer.cancel();
    }

private:
    void start_read_socket() {
        using namespace std::literals;
        asio::async_read_until(sock, asio::dynamic_buffer(rx_buffer), "\n"sv, track([this] (const std::error_code& ec, size_t bytes_transferred) {
            if (ec) {
                return complete(events::failure(ec));
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

                return complete(events::failure(ec));
            }

            return complete(events::failure(make_error_code(std::errc::timed_out)));
        }));
    }
private:
    asio::ip::tcp::socket   sock;
    asio::steady_timer      timer;
    bool                    extend;
    std::string             rx_buffer;
};

class backoff : public state<events::retry, events::failure, events::shutdown> {
public:
    backoff(asio::io_service& io, std::chrono::seconds cooldown, completion_handler cb) :
        state(io, std::move(cb)),
        timer(io)
    {
        timer.expires_from_now(cooldown);
        timer.async_wait(track([this](const std::error_code& ec) {
            ec ? complete(events::failure(ec)) : complete(events::retry{});
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

class client : public fsm<transitions<
        transition<resolving, events::failure, backoff>,
        transition<resolving, asio::ip::tcp::endpoint, connecting>,
        transition<resolving, events::shutdown, completed>,

        transition<connecting, events::failure, backoff>,
        transition<connecting, std::reference_wrapper<asio::ip::tcp::socket>, online>,
        transition<connecting, events::shutdown, completed>,

        transition<online, events::failure, backoff>,
        transition<online, events::shutdown, completed>,

        transition<backoff, events::retry, resolving>,
        transition<backoff, events::failure, completed>,
        transition<backoff, events::shutdown, completed>
    >>
{
public:
    client(asio::io_service& io) : io(io) {}
    using completion_handler = std::function<void(const std::error_code& r)>;

    void async_wait(const std::string& host, const std::string& service, completion_handler cb) {
        sess.emplace(std::move(cb), std::bind(&client::on_event<resolving, resolving::result>, this, std::placeholders::_1), io, host, service);
    }
private:

    void complete(const std::error_code& ec = std::error_code{}) {
        if (sess) {
            io.post(std::bind(sess->cb, ec));
            sess = std::nullopt;
        }
    }

    template<typename State, typename Event>
    void on_event(const Event& ev) {
        std::visit([this](auto v) {
            using event_type = std::decay_t<decltype(v)>;
            transition_table::assert_match<State, event_type>();
            using next_state_type = transition_table::next_state<State, event_type>;
            using namespace boost::core;
            log("%s + %s => %s", type_name<State>(), type_name<event_type>(), type_name<next_state_type>());
            if constexpr (!std::is_same_v<next_state_type, completed>) {
                auto cb = std::bind(&client::on_event<next_state_type, typename next_state_type::result>, this, std::placeholders::_1);
                sess->st = state_factory<next_state_type, event_type, context>{}(v, sess->ctx, std::move(cb));
                return;
            } else if constexpr (std::is_same_v<event_type, events::shutdown>) {
                complete(v.ec);
                return;
            } else {
                complete();
                return;
            }
        }, ev);
    }
private:
    struct session {
        template<typename Callback, typename ...Args>
        session(completion_handler cb, Callback state_cb, Args&& ...args) :
            cb(std::move(cb)),
            ctx(std::forward<Args>(args)...),
            st(state_factory<resolving, std::monostate, context>{}(std::monostate{}, ctx, std::move(state_cb)))
        {}

        completion_handler          cb;
        context                     ctx;
        std::unique_ptr<state_base> st;
    };

    std::optional<session>      sess;
    asio::io_service&           io;
};