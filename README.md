# asio-fsm
Finite State Machine programming pattern for C++/ASIO

## Project status
The project is in pre-alpha state. I am experimenting with the concept and try prove that it covers usecases properly.

## Motivation
While creating Asio programs, usually we're using the mental concept of a Finite State Machine (FSM). Multiple approaches are possible to implement an asio-based program in terms of FSMs, but all of these contain a huge amount of boilerplate, restrict reusing and have other drawbacks. Asio tries to overcome these drawbacks by providing helper classes that make the heavy lifting and ease the mapping process from an FSM to the code which implements that.

## Documentation
See the wiki: https://github.com/andreihu/asio-fsm/wiki

## Requirements
Since the library is header only, nothing is needed to use it beyond a C++17 compliant compiler (checked with g++ and clang++) and Asio itself.

### Optional requirements
- CMake (to build examples)
- Boost (to build examples)

  
## License
BSD
