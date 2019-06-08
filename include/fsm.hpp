#include <asio.hpp>

#include <variant>
#include <system_error>
#include <functional>
#include <type_traits>
#include "helpers.hpp"

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
            scope_exit _([this] {
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

template<typename State, typename Event, typename Context>
struct state_factory {
    auto operator()(Event ev, Context& ctx) const {
        return std::make_tuple(std::ref(ctx.io), ev);
    }
};


template<typename Event, typename Context>
struct result_factory {
    auto operator()(Event ev, Context&) const {
        return ev;
    }
};

template<typename SourceState, typename Event, typename NextState>
struct transition {
    using source = SourceState;
    using event = Event;
    using next = NextState;
};

template<typename State, typename Event>
constexpr bool match_impl() {
    return false;
}

template<typename State, typename Event, typename Transition>
constexpr bool match_impl() {
    return std::is_same_v<State, typename Transition::source> && std::is_same_v<Event, typename Transition::event>;
}

template<typename State, typename Event, typename Transition, typename Transition2, typename ...Args2>
constexpr bool match_impl() {
    return match_impl<State, Event, Transition>() || match_impl<State, Event, Transition2, Args2...>();
}

struct end_of_list;

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
    using next_state = typename next_state_impl<State, Event, Args..., end_of_list>::type;
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
    struct next_state_impl<State, Event, Transition, end_of_list> {
        using type = typename std::conditional<
            match_impl<State, Event, Transition>(),
            typename Transition::next,
            no_transition
        >::type;
    };
};

template<typename Result, typename StartState, typename EndState, typename Context, typename Transitions>
class fsm;

template<typename StartState, typename EndState, typename Transitions>
class fsm_assertions;

template<typename StartState, typename EndState, typename ...Args>
class fsm_assertions<StartState, EndState, transitions<Args...>> {
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

template<typename Variant, typename T>
struct variant_unique_append;

template<typename ...Args, typename T>
struct variant_unique_append<std::variant<Args...>, T> {
    using type = typename std::conditional<
        contains<T, Args...>::value,
        std::variant<Args...>,
        std::variant<Args..., T>
    >::type;
};

template<typename UniqueStates, typename Transition, typename ...Rest>
struct state_holder;

template<typename ...Args, typename StartState, typename Event, typename NextState, typename ...Rest>
struct state_holder<std::variant<Args...>, transition<StartState, Event, NextState>, Rest...> {
private:
    using tmp_type = typename variant_unique_append<std::variant<Args...>, StartState>::type;
    using tmp_type2 = typename variant_unique_append<tmp_type, NextState>::type;
public:
    using type = typename state_holder<tmp_type2, Rest...>::type;
};

template<typename ...Args, typename StartState, typename Event, typename NextState>
struct state_holder<std::variant<Args...>, transition<StartState, Event, NextState>, end_of_list> {
private:
    using tmp_type = typename variant_unique_append<std::variant<Args...>, StartState>::type;
    using tmp_type2 = typename variant_unique_append<tmp_type, NextState>::type;
public:
    using type = tmp_type2;
};

template<typename Result, typename StartState, typename EndState, typename Context, typename ...Args>
class fsm<Result, StartState, EndState, Context, transitions<Args...>> :
    fsm_assertions<StartState, EndState, transitions<Args...>> {
public:
    using start_state = StartState;
    using end_state = EndState;
    using context = Context;
    using result = Result;
    using completion_handler = std::function<void(const result&)>;
    using transition_table = transitions<Args...>;
    using state_storage = typename state_holder<std::variant<std::monostate, StartState>, Args..., end_of_list>::type;

    fsm(asio::io_service& io) : io(io) {}

    template<typename ...Args2>
    void async_wait(completion_handler cb, Args2 ...args) {
        if (sess) {
            return io.post(std::bind(cb, make_error_code(std::errc::operation_in_progress)));
        }

        sess.emplace(io, std::move(cb), std::forward<Args2>(args)...);
        auto& active_state = sess->active_state();
        std::apply([&](auto&& ...args2) {
            active_state.template emplace<start_state>(std::forward<decltype(args2)>(args2)...);            
        }, state_factory<start_state, std::monostate, context>{}(std::monostate{}, sess->ctx));
        std::visit([&](auto&& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same<T, start_state>()) {
                s.async_wait(std::bind(&fsm::on_event<start_state, typename start_state::result>, this, std::placeholders::_1));
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
    static void visit(Visitor& visitor) {
        visitor.template start<fsm>();
        visit_impl<Visitor, Args..., end_of_list>{}(visitor);
        visitor.template end<fsm>();
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
    struct visit_impl<Visitor, Transition, end_of_list> {
        void operator()(Visitor& visitor) const {
            visitor.template operator()<Transition>();
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
            using namespace boost::core;
            log("%s + %s => %s", type_name<State>(), type_name<event_type>(), type_name<next_state_type>());
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
                        s.async_wait(std::bind(&fsm::on_event<next_state_type, typename next_state_type::result>, this, std::placeholders::_1));
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
};
