#pragma once

// ours
#include "transition.hpp"
#include "transitions.hpp"
#include "result_factory.hpp"
#include "state_factory.hpp"
#include "detail/end_of_list.hpp"
#include "detail/state_machine_assertions.hpp"
#include "detail/state_holder.hpp"
#include "util/type_name.hpp"
#include "util/contains.hpp"

// thirdparty
#include <asio.hpp>

// std
#include <exception>
#include <functional>
#include <type_traits>
#include <variant>

namespace afsm {




template<typename Traits>
class state_machine : private detail::state_machine_assertions<typename Traits::start_state, typename Traits::end_state, typename Traits::transitions> {
public:
    using start_state = typename Traits::start_state;
    using end_state = typename Traits::end_state;
    using context = typename Traits::context;
    using result = typename Traits::result;
    using completion_handler = std::function<void(const result&)>;
    using transition_table = typename Traits::transitions;
    using state_storage = typename detail::state_holder<std::variant<std::monostate, start_state>, transition_table>::type;

    state_machine(asio::io_service& io) : io(io) {}

    template<typename ...Args2>
    void async_wait(completion_handler cb, Args2&& ...args) {
        if (sess) {
            throw std::runtime_error("state machine already active");
        }

        sess.emplace(io, std::move(cb), std::forward<Args2>(args)...);
        auto& active_state = sess->active_state();
        std::apply([&](auto&& ...args2) {
            active_state.template emplace<start_state>(std::forward<decltype(args2)>(args2)...);
        }, state_factory<start_state, std::monostate, context>{}(std::monostate{}, sess->ctx));
        std::visit([&](auto&& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same<T, start_state>()) {
                s.async_wait(std::bind(&state_machine::on_event<start_state, typename start_state::result>, this, std::placeholders::_1));
            }
        }, active_state);
    }

    void cancel() {
        if (!sess) {
            return;
        }

        std::visit([](auto&& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (!std::is_same_v<std::monostate, T> && !std::is_same_v<end_state, T>) {
                s.cancel();
            }
        }, sess->active_state());
    }

    template<typename Visitor>
    static void static_visit(Visitor& visitor) {
        visit<Visitor, transition_table>{}(visitor);
    }
private:
    template<typename Visitor, typename Transition, typename ...Rest>
    struct visit_impl {
        void operator()(Visitor& visitor) const {
            visitor.template operator()<Transition>();
            visit_impl<Visitor, Rest...>{}(visitor);
        }
    };

    template<typename Visitor, typename Transition>
    struct visit_impl<Visitor, Transition, detail::end_of_list> {
        void operator()(Visitor& visitor) const {
            visitor.template operator()<Transition>();
        }
    };

    template<typename Visitor, typename Transitions>
    struct visit;

    template<typename Visitor, typename ...Args>
    struct visit<Visitor, transitions<Args...>> {
        void operator()(Visitor& visitor) const {
            visitor.template start<state_machine>();
            visit_impl<Visitor, Args..., detail::end_of_list>{}(visitor);
            visitor.template end<state_machine>();
        }
    };

    struct session {
        template<typename ...Args2>
        session(asio::io_service& io, completion_handler cb, Args2&& ...args) :
            cb(std::move(cb)),
            ctx(io, std::forward<Args2>(args)...),
            holder1_active(true)
            {}

        state_storage& active_state() {
            return holder1_active ? st1 : st2 ;
        }

        completion_handler              cb;
        context                         ctx;
        // std::unique_ptr<state_base>     st;
        state_storage                   st1;
        state_storage                   st2;
        bool                            holder1_active;
    };
    using opt_session = std::optional<session>;

    void complete(result r) {
        if (sess) {
            io.post(std::bind(sess->cb, std::move(r)));
            sess = std::nullopt;
        }
    }

    template<typename State, typename Event>
    void on_event(const Event& ev) {
        std::visit([this](auto v) {
            using event_type = std::decay_t<decltype(v)>;
            transition_table::template assert_match<State, event_type>();
            using next_state_type = typename transition_table::template next_state<State, event_type>;
            // log("{} + {} => {}", type_name<State>(), type_name<event_type>(), type_name<next_state_type>());
            if constexpr (!std::is_same_v<next_state_type, end_state>) {
                auto& old_state = sess->active_state();
                // creating the new state
                sess->holder1_active = !sess->holder1_active;
                auto& active_state = sess->active_state();
                std::apply([&](auto&& ...args2) {
                    active_state.template emplace<next_state_type>(std::forward<decltype(args2)>(args2)...);
                }, state_factory<next_state_type, event_type, context>{}(v, sess->ctx));
                // invoking async_wait on the new state
                std::visit([&](auto&& s) {
                    using T = std::decay_t<decltype(s)>;
                    if constexpr (std::is_same_v<T, next_state_type> && !std::is_same_v<T, end_state>) {
                        s.async_wait(std::bind(&state_machine::on_event<next_state_type, typename next_state_type::result>, this, std::placeholders::_1));
                    }
                }, active_state);
                // throwing away the old state
                old_state = std::monostate{};
            } else {
                complete(result_factory<event_type,context>{}(v, sess->ctx));
            }
        }, ev);
    }
private:
    asio::io_service&   io;
    opt_session         sess;
}; // state_machine

} // namespace afsm