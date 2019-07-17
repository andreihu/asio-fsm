#pragma once

namespace afsm {

template<typename Event, typename Context>
struct result_factory {
    auto operator()(Event ev, Context&) const {
        return ev;
    }
};

} // namespace afsm