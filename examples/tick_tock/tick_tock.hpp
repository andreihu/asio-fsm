#pragma once

#include "states.hpp"
#include "events.hpp"

#include <fsm/fsm.hpp>

struct context {
    context(asio::io_service& io) : io(io) {}
    asio::io_service& io;
};

template<typename Event>
struct state_factory<ticked, Event, context> {
    auto operator()(const Event&, context& ctx) const {
        return std::make_tuple(std::ref(ctx.io));
    }
};

template<typename Event>
struct state_factory<tocked, Event, context> {
    auto operator()(const Event&, context& ctx) const {
        return std::make_tuple(std::ref(ctx.io));
    }
};

struct tick_tock_traits {
    using start_state = ticked;
    using end_state = completed;
    using context = ::context;
    using result = std::error_code;
    using transitions = ::transitions<
        transition<ticked, tock, tocked>,
        transition<ticked, terminated, completed>,
        transition<tocked, tick, ticked>,
        transition<tocked, terminated, completed>
    >;
};

using tick_tock = fsm<tick_tock_traits>;