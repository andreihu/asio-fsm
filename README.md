# asio-fsm
Finite State Machine programming pattern for C++/ASIO

## Project status
The project is in pre-alpha state. I am experimenting with the concept and try prove that it covers usecases properly.

## Motivation
While creating Asio programs, usually we're using the mental concept of a Finite State Machine (FSM).

### A legacy approach
Let's consider the following state machine of a fault tolerant peer class:
- Resolving + [Resolved] => Connecting
- Resolving + [Failed] => Backoff
- Resolving + [Stopped] => Completed
- Online + [Connected] => Online
- Online + [Failed] => Backoff
- Online + [Stopped] => Completed
- Connected + [Failed] => Backoff
- Connected + [Stopped] => Completed
- Backoff + [Timeout] => Resolving
- Backoff + [Stopped] => Completed

State machines like that are usually implemented in a "monolithic" class. Something along the lines of:

```
class peer {
public:
    peer(asio::io_service& io);
    using completion_handler = std::function<void(const std::error_code& ec)>;
    void async_wait(const std::string host, uint16_t port, completion_handler cb);
    void cancel();
private:
    enum class state {
        resolving,
        connecting,
        online,
        backoff,
        completed
    };

    void set_state(state new_state) {
        switch(new_state) {
            case state::resolving:
                return on_enter_resolving();
            case state::connecting:
                return on_enter_connecting();
            case state::backoff:
                return on_enter_backoff();
            case state::completed:
                return on_enter_completed();
        }
    }

    void on_enter_resolving();
    void on_enter_connecting();
    void on_enter_online();
    void on_enter_backoff();
    void on_enter_completed();
private:
    using proto = asio::ip::tcp;
    struct session {
        completion_handler cb;
        std::string        host;
        uint16_t           port;
        proto::resolver    resolver;
        proto::endpoint    ep;
        proto::socket      socket;
    };

    asio::io_service& io;
    std::optional<session>;
};
```

This implementation pattern has several drawbacks:
- Enormous boilerplate (e.g. the set_state() method)
- State superposition in the session class: all io objects, intermediate contextual variables and parameters are stored in the context. It's hard to establish well defined pre/post conditions for the session class.
- Implementation of some methods might be tricky. The cancel() operation has to wait for all async ops to complete before posting the completion handler or use some form of refcounting (typically binding shared_from_this() into callbacks).
- It's hard to verify if the state machine is compliant to the designed one, and no static assertions can be performed on it.
- Since it's a single class, proliferation of states and events in the state machine results a spaghetti-class which is hard to maintain.

## How this library helps?
This library provides helpers to implement state machines via a different pattern.

### Concepts
- Individual states should be encapsulated into their own classes.
- Events are represented as types as well.
- States should be loosely coupled in a sense that they could be reused on their own.
- Contextual information should be kept outside the states in a context class.
- Perform compile time checks on state machine sanity (unhandled events, end state reachability) and give helpful diagnostics

### Example
Refer to the implementation in examples/tcp_client

## Requirements
Since the library is header only, nothing is needed to use it beyond a C++17 compliant compiler (checked with g++ and clang++) and Asio itself.

### Optional requirements
- CMake (to build examples)
- Boost (to build examples)

  
## License
BSD
