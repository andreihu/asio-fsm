# asio-fsm
Finite State Machine programming pattern for C++/ASIO

## Motivation
While creating Asio programs, usually we're using the mental concept of a Finite State Machine (FSM).

### A legacy approach
Let's consider the following state machine of a fault tolerant peer class:
- Resolving + [Resolved] => Connecting
- Resolving + [Failed] => Backoff
- Resolving + [Stopped] => Completed
- Connecting + [Connected] => Online
- Connecting + [Failed] => Backoff
- Connection + [Stopped] => Completed
- Connected + [Failed] => Backoff
- Connected + [Stopped] => Completed

## Design concepts:
- Individual states should be encapsulated into their own classes.
- States should be loosely coupled in a sense that they could be reused on their own
- Perform compile time checks on state machine sanity (unhandled events, end state reachability) and give helpful diagnostics

## Requirements
- C++17 compliant compiler (checked with g++ and clang++)
- Boost (optional)

## License
BSD
