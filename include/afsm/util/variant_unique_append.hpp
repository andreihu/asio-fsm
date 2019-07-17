#pragma once

// ours
#include "contains.hpp"

// std
#include <type_traits>
#include <variant>

namespace afsm {
namespace util {

template<typename Variant, typename T>
struct variant_unique_append;

template<typename ...Args, typename T>
struct variant_unique_append<std::variant<Args...>, T> {
    using type = typename std::conditional<
        util::contains<T, Args...>::value,
        std::variant<Args...>,
        std::variant<Args..., T>
    >::type;
};

} // namespace util
} // namespace afsm