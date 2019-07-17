#pragma once

namespace afsm {
namespace util {

template<typename Callable>
struct scope_exit {
    scope_exit(Callable f) : f(std::move(f)) {}
    scope_exit(const scope_exit&) = delete;
    scope_exit& operator=(const scope_exit&) = delete;
    ~scope_exit(void) { f(); }
private:
    Callable f;
};

} // namespace util
} // namespace afsm
