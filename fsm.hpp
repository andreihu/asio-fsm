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


template<typename State, typename Event, typename Context>
struct state_factory {
    std::unique_ptr<state_base> operator()(const Event& ev, Context& ctx) const {
        return nullptr;
    }
};

template<typename SourceState, typename Event, typename NextState>
struct transition {
    using source = SourceState;
    using event = Event;
    using next = NextState;
};

template<typename ...Args>
struct transitions {
    template<typename State, typename Event, typename Context>
    static std::unique_ptr<state_base> make_transition(const Event& ev, Context& ctx) {
        assert_match<State, Event>(ev);

        return make_transition_impl<State, Event, Context, Args...>(ev, ctx);
    }
private:
    template<typename State, typename Event, typename Context>
    static std::unique_ptr<state_base> make_transition_impl(const Event& ev, Context& ctx) {
        return nullptr;
    }

    template<typename State, typename Event, typename Context, typename Transition>
    static std::unique_ptr<state_base> make_transition_impl(const Event& ev, Context& ctx) {
        if constexpr (match_impl<State, Event, Transition>()) {
            return state_factory<typename Transition::next, Event, Context>{}(ev, ctx);
        }
        return nullptr;
    }

    template<typename State, typename Event, typename Context, typename Transition, typename Transition2, typename ...Args2>
    static std::unique_ptr<state_base> make_transition_impl(const Event& ev, Context& ctx) {
        if (auto retval = make_transition_impl<State, Event, Context, Transition>(ev, ctx)) {
            return retval;
        } else if (auto retval = make_transition_impl<State, Event, Context, Transition>(ev, ctx)) {
            return retval;
        }

        return make_transition_impl<State, Event, Context, Args2...>(ev, ctx);
    }

    template<typename State, typename Event>
    static void assert_match(const Event& ev) {
        static_assert(match_impl<State, Event, Args...>(), "no matching transition found");
    }

    template<typename State, typename Event>
    static constexpr bool match_impl() {
        return false;
    }

    template<typename State, typename Event, typename Transition>
    static constexpr bool match_impl() {
        return std::is_same_v<State, typename Transition::source> && std::is_same_v<Event, typename Transition::event>;
    }

    template<typename State, typename Event, typename Transition, typename Transition2, typename ...Args2>
    static constexpr bool match_impl() {
        return match_impl<State, Event, Transition>() || match_impl<State, Event, Transition2>() || match_impl<State, Event, Args2...>();
    }
};

struct completed {};