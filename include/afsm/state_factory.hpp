#pragma once

#include <functional>
#include <tuple>

namespace afsm {

template<typename State, typename Event, typename Context>
struct state_factory {
    auto operator()(Event ev, Context& ctx) const {
        return std::make_tuple(std::ref(ctx.io), ev);
    }
};

} // namespace afsm