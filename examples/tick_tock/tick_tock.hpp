#pragma once

#include "states.hpp"
#include "events.hpp"

#include <fsm/fsm.hpp>
#include <null_context.hpp>

struct tick_tock_traits {
    using start_state = ticked;
    using end_state = terminated;
    using context = null_context;
    using result = std::error_code;
    using transition_table = transitions<
        transition<ticked, tock, tocked>,
        transition<ticked, terminated, completed>,
        transition<tocked, tick, ticked>,
        transition<tocked, terminated, completed>
    >;
};

using tic_tock = fsm<tick_tock_traits>;