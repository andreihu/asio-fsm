#pragma once

// ours
#include "events.hpp"
#include "states.hpp"

#include <afsm/state_machine.hpp>

// thirdparty
#include <asio.hpp>

// std
#include <string>
#include <cstdint>

struct context {
    context(asio::io_service& io, const std::string& host, const std::string& service) : io(io), host(host), service(service), nbackoff(0) {}
    asio::io_service&       io;
    std::string             host;
    std::string             service;
    size_t                  nbackoff;
};

namespace afsm {

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

} // namespace afsm

struct client_traits {
    using start_state = resolving;
    using end_state = completed;
    using result = std::error_code;
    using context = ::context;
    using transitions = afsm::transitions<
        afsm::transition<resolving, failed, backoff>,
        afsm::transition<resolving, resolved, connecting>,
        afsm::transition<resolving, terminated, completed>,

        afsm::transition<connecting, failed, backoff>,
        afsm::transition<connecting, connected, online>,
        afsm::transition<connecting, terminated, completed>,

        afsm::transition<online, failed, backoff>,
        afsm::transition<online, terminated, completed>,

        afsm::transition<backoff, retry, resolving>,
        afsm::transition<backoff, failed, completed>,
        afsm::transition<backoff, terminated, completed>
    >;
};

using client = afsm::state_machine<client_traits>;
