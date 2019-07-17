#pragma once

// ours
#include "end_of_list.hpp"
#include <afsm/transition.hpp>
#include <afsm/util/variant_unique_append.hpp>

// std
#include <variant>

namespace afsm {
namespace detail {

template<typename UniqueStates, typename Transition, typename ...Rest>
struct state_holder_impl;

template<typename ...Args, typename StartState, typename Event, typename NextState, typename ...Rest>
struct state_holder_impl<std::variant<Args...>, transition<StartState, Event, NextState>, Rest...> {
private:
    using tmp_type = typename util::variant_unique_append<std::variant<Args...>, StartState>::type;
    using tmp_type2 = typename util::variant_unique_append<tmp_type, NextState>::type;
public:
    using type = typename state_holder_impl<tmp_type2, Rest...>::type;
};

template<typename ...Args, typename StartState, typename Event, typename NextState>
struct state_holder_impl<std::variant<Args...>, transition<StartState, Event, NextState>, end_of_list> {
private:
    using tmp_type = typename util::variant_unique_append<std::variant<Args...>, StartState>::type;
    using tmp_type2 = typename util::variant_unique_append<tmp_type, NextState>::type;
public:
    using type = tmp_type2;
};

template<typename UniqueStates, typename TransitionTable>
struct state_holder;

template<typename UniqueStates, typename ...Args>
struct state_holder<UniqueStates, transitions<Args...>>
{
    using type = typename state_holder_impl<UniqueStates, Args..., end_of_list>::type;
};

} // namespace detail
} // namespace afsm