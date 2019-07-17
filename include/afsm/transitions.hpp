#pragma once

// ours
#include "detail/match_impl.hpp"
#include "detail/end_of_list.hpp"

// std
#include <type_traits>

namespace afsm {

template<typename ...Args>
class transitions {
private:
    template<typename State, typename Event, typename Transition, typename ...Rest>
    struct next_state_impl;
public:
    struct no_transition;

    template<typename State, typename Event>
    static void assert_match() {
        static_assert(match<State, Event>(), "no matching transition found");
    }

    template<typename State, typename Event>
    static constexpr bool match() {
        return match_impl<State, Event, Args...>();
    }

    template<typename State, typename Event>
    using next_state = typename next_state_impl<State, Event, Args..., detail::end_of_list>::type;
private:
    template<typename State, typename Event, typename Transition, typename ...Rest>
    struct next_state_impl {
        using type = typename std::conditional<
            match_impl<State, Event, Transition>(),
            typename Transition::next,
            typename next_state_impl<State, Event, Rest...>::type
        >::type;
    };

    template<typename State, typename Event, typename Transition>
    struct next_state_impl<State, Event, Transition, detail::end_of_list> {
        using type = typename std::conditional<
            match_impl<State, Event, Transition>(),
            typename Transition::next,
            no_transition
        >::type;
    };
};

} // namespace afsm