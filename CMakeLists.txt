cmake_minimum_required (VERSION 3.8)

project(asio-fsm)

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
  conan_basic_setup(TARGETS)
else()
  message(WARNING "you can initialize you dependancies with 'conan install ..'")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)

add_library(fsm INTERFACE)
target_include_directories(fsm INTERFACE include)
target_link_libraries(fsm INTERFACE CONAN_PKG::fmt CONAN_PKG::asio)

add_subdirectory(examples)
