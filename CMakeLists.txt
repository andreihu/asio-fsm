cmake_minimum_required (VERSION 3.8)

project(asio_presentation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_executable(client client.cpp)
target_compile_definitions(client PRIVATE ASIO_STANDALONE)
target_include_directories(client PRIVATE /usr/local/include)
target_link_libraries(client Threads::Threads)
