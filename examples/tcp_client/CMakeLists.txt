add_executable(tcp_client
  main.cpp
  events.hpp
  states.hpp
  tcp_client.hpp
)
target_link_libraries(tcp_client PRIVATE fsm)
