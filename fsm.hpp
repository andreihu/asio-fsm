#include <asio.hpp>

#include <variant>
#include <system_error>
#include <functional>

class state_base {
public:
    state_base(asio::io_service& io) : io(io) {}
    virtual void cancel() = 0;
    virtual ~state_base()  = default;
protected:
    asio::io_service&       io;
}; // class state_base

template<typename T>
class state : public state_base {
public:
    using result = std::variant<std::error_code, T>;
    using completion_handler = std::function<void(const result&)>;
    state(asio::io_service& io, completion_handler cb) : state_base(io), cb(std::move(cb)), rc(0) {}
    virtual ~state() override = default;

    template<typename V>
    void complete(V&& v) {
        if (!res) {
            res.emplace(std::forward<V>(v));
            cancel();
        }
    }

    template<class Callable>
    auto track(Callable&& callable) {
        ++rc;
        return [this, callable = std::forward<Callable>(callable)](auto&&... args) -> decltype(auto) {
            scope_exit _([this] {
                if (--rc == 0) {
                    io.post(std::bind(cb, *res));
                }
            });
            return callable(std::forward<decltype(args)>(args)...);
        };
    }
private:
    completion_handler          cb;
    std::optional<result>       res;
    size_t                      rc;
}; // class state

struct completed{
    completed(asio::io_service& io, const std::error_code& ec) {}
};

template<typename State, typename Event, typename Context>
struct state_factory {
    template<typename Callback>
    std::unique_ptr<state_base> operator()(const Event& ev, Context& ctx, Callback cb) const {
        return std::make_unique<State>(ctx.io, ev, std::move(cb));
    }
};

template<typename Context>
struct state_factory<completed, std::error_code, Context> {
    std::unique_ptr<state_base> operator()(const std::error_code& ev, Context& ctx) const {
        ctx.io.post(std::bind(ctx.cb, ev));
        return nullptr;
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


template<typename ...Args>
struct transitions {
    struct no_transition {};
    struct end_of_list;

    template<typename State, typename Event>
    static void assert_match() {
        static_assert(match<State, Event>(), "no matching transition found");
    }

    template<typename State, typename Event>
    static constexpr bool match() {
        return match_impl<State, Event, Args...>();
    }


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

    template<typename State, typename Event>
    using next_state = typename next_state_impl<State, Event, Args..., end_of_list>::type;


private:
#if 0
    template<typename State, typename Event, typename Context, typename Callback>
    static std::unique_ptr<state_base> make_transition(const Event& ev, Context& ctx, Callback cb) {
        assert_match<State, Event>(ev);

        return make_transition_impl<State, Event, Context, Args...>(ev, ctx, std::move(cb));
    }


    template<typename State, typename Event, typename Context, typename Callback>
    static std::unique_ptr<state_base> make_transition_impl(const Event& ev, Context& ctx, Callback cb) {
        return nullptr;
    }

    template<typename State, typename Event, typename Context, typename Callback, typename Transition>
    static std::unique_ptr<state_base> make_transition_impl(const Event& ev, Context& ctx, Callback cb) {
        if constexpr (match<State, Event, Transition>()) {
            return state_factory<typename Transition::next, Event, Context>{}(ev, ctx, std::move(cb));
        }
        return nullptr;
    }

    template<typename State, typename Event, typename Context, typename Callback, typename Transition, typename Transition2, typename ...Args2>
    static std::unique_ptr<state_base> make_transition_impl(const Event& ev, Context& ctx, Callback cb) {
        if (auto retval = make_transition_impl<State, Event, Context, Transition>(ev, ctx, std::move(cb))) {
            return retval;
        } else if (auto retval = make_transition_impl<State, Event, Context, Callback, Transition2>(ev, ctx, std::move(cb))) {
            return retval;
        }

        return make_transition_impl<State, Event, Context, Callback, Args2...>(ev, ctx, std::move(cb));
    }
#endif
};