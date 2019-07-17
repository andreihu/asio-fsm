#pragma once

// ours
#include "states.hpp"
#include "events.hpp"

#include <afsm/state_machine.hpp>

struct context {
    context(asio::io_service& io) : io(io) {}
    asio::io_service& io;
};

namespace afsm {
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

} // namespace afsm

struct tick_tock_traits {
    using start_state = ticked;
    using end_state = completed;
    using context = ::context;
    using result = std::error_code;
    using transitions = afsm::transitions<
        afsm::transition<ticked, tock, tocked>,
        afsm::transition<ticked, terminated, completed>,
        afsm::transition<tocked, tick, ticked>,
        afsm::transition<tocked, terminated, completed>
    >;
};

using tick_tock = afsm::state_machine<tick_tock_traits>;