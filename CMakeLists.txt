cmake_minimum_required(VERSION 3.16)
project(Caitsidhe_V LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Caitsidhe_V src/main.cpp)
target_include_directories(Caitsidhe_V PRIVATE src)

include(CTest)
if(BUILD_TESTING)
    add_executable(caitsidhe_tests tests/test_cpu.cpp)
    target_include_directories(caitsidhe_tests PRIVATE src)
    add_test(NAME caitsidhe_smoke_tests COMMAND caitsidhe_tests)
endif()
