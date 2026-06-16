cmake_minimum_required(VERSION 3.16)
project(circuit_simulator_dsl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(circuit_simulator_lib
    src/circuit_simulator.cpp
)

target_include_directories(circuit_simulator_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(circuit_cli
    main.cpp
    src/cli.cpp
)

target_link_libraries(circuit_cli
    PRIVATE
        circuit_simulator_lib
)