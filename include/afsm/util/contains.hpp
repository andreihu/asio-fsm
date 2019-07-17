#pragma once

#include <type_traits>

namespace afsm {
namespace util {

// returns a constexpr true if the pack Args contains What
template<typename What, typename ...Args>
struct contains {
    static constexpr bool value {(std::is_same_v<What, Args> || ...)};
};

} // namespace util
} // namespace afsm
