cmake_minimum_required(VERSION 3.0.0)
set(CMAKE_CXX_STANDARD 11)

project(SMASH VERSION 0.1.0 LANGUAGES CXX)

set(FLAGS_DEBUG "-std=c++11 --pedantic-errors -Wall -Werror")
set(FLAGS_RELEASE "${FLAGS_DEBUG} -DNDEBUG")

set(CMAKE_CPP_FLAGS ${FLAGS_DEBUG})

set(CPP_FILES Commands.cpp)
set(CPP_FILES ${CPP_FILES} signals.cpp)

add_executable(smash smash.cpp ${CPP_FILES})
