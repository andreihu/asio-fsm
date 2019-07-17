#pragma once

#include "util/scope_exit.hpp"

// thirdparty
#include <asio.hpp>

// std
#include <functional>
#include <optional>
#include <variant>

namespace afsm {

class state_base {
public:
    state_base(asio::io_service& io) : io(io) {}
    virtual void cancel() = 0;
    virtual ~state_base()  = default;
protected:
    asio::io_service&       io;
}; // class state_base

template<typename ...Events>
class state : public state_base {
public:
    using result = std::variant<Events...>;
    using completion_handler = std::function<void(const result&)>;
    state(asio::io_service& io) : state_base(io), rc(0) {}

    void async_wait(completion_handler cb) {
        if (this->cb) {
            throw std::runtime_error("state is already active");
        }

        this->cb = std::move(cb);
        on_enter();
    }

    virtual void on_enter() = 0;
    virtual ~state() override = default;

    template<typename V, typename ...Args>
    void complete(Args&& ...args) {
        if (!res) {
            res.emplace(std::in_place_type<V>, std::forward<Args>(args)...);
            cancel();
        }
    }

    template<class Callable>
    auto track(Callable&& callable) {
        ++rc;
        return [this, callable = std::forward<Callable>(callable)](auto&&... args) -> decltype(auto) {
            util::scope_exit _([this] {
                if (--rc == 0) {
                    if (cb) {
                        io.post(std::bind(*cb, *res));
                        cb = std::nullopt;
                        res = std::nullopt;
                    }
                }
            });
            return callable(std::forward<decltype(args)>(args)...);
        };
    }
private:
    std::optional<completion_handler>       cb;
    std::optional<result>                   res;
    size_t                                  rc;
}; // class state

} // namespace afsm
