cmake_minimum_required (VERSION 3.8)

project(asio_presentation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

include_directories(/usr/local/include include)
add_compile_definitions(ASIO_STANDALONE)
link_libraries(Threads::Threads)
add_subdirectory(examples)
