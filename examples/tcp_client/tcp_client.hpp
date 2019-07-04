#pragma once

#include "events.hpp"
#include "states.hpp"

#include <fsm.hpp>

#include <asio.hpp>

struct context {
    context(asio::io_service& io, const std::string& host, const std::string& service) : io(io), host(host), service(service), nbackoff(0) {}
    asio::io_service&       io;
    std::string             host;
    std::string             service;
    size_t                  nbackoff;
};

template<typename Event>
struct state_factory<resolving, Event, context> {
    auto operator()(const Event&, context& ctx) const {
        return std::make_tuple(std::ref(ctx.io), ctx.host, ctx.service);
    }
};

template<typename Event>
struct state_factory<online, Event, context> {
    auto operator()(const Event& ev, context& ctx) const {
        ctx.nbackoff = 0;
        return std::make_tuple(std::ref(ctx.io), ev);
    }
};

template<typename Event>
struct state_factory<backoff, Event, context> {
    auto operator()(const Event&, context& ctx) const {
        std::chrono::seconds timeout(std::min<int>(16, std::pow(2, ctx.nbackoff++)));
        return std::make_tuple(std::ref(ctx.io), timeout);
    }
};

struct client_traits {
    using start_state = resolving;
    using end_state = completed;
    using result = std::error_code;
    using context = ::context;
    using transitions = ::transitions<
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
    >;
};

using client = fsm<client_traits>;
