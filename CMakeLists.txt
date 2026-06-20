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

#simAll
add_executable(simAll
    simAll.cpp
)

target_link_libraries(simAll
    PRIVATE
        circuit_simulator_lib
)

add_executable(test_link_api
    test_link_api.cpp
)

target_link_libraries(test_link_api
    PRIVATE
        circuit_simulator_lib
)