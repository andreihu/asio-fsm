#pragma once 

#include "../transitions.hpp"

namespace afsm {
namespace detail {

template<typename StartState, typename EndState, typename Transitions>
class state_machine_assertions;

template<typename StartState, typename EndState, typename ...Args>
class state_machine_assertions<StartState, EndState, transitions<Args...>> {
private:
    constexpr static bool end_state_reachable() {
        // TODO: implement me
        return true;
    }

    constexpr static bool all_state_reachable() {
        // TODO: implement me
        return true;
    }

    static_assert(!std::is_same_v<StartState, EndState>, "start state and end state must be different");
    static_assert(end_state_reachable(), "end state must be reachable from start state");
};

} // namespace detail
} // namespace afsm