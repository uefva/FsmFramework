cmake_minimum_required(VERSION 3.10)

project(FsmFramework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Boost CONFIG REQUIRED COMPONENTS json system)
enable_testing()

set(REG_FSM_CORE_SOURCES
    reg_fsm/src/AuthFactory.cpp
    reg_fsm/src/AuthFsm.cpp
    reg_fsm/src/CMsg.cpp
    reg_fsm/src/Cfsm.cpp
    reg_fsm/src/Cfactory.cpp
    reg_fsm/src/Cfactory_mgr.cpp
    reg_fsm/src/Logger.cpp
    reg_fsm/src/RegFactory.cpp
    reg_fsm/src/RegFsm.cpp
    reg_fsm/src/TimerManager.cpp
)

set(REG_FSM_HTTP_SOURCES
    reg_fsm/src/HttpServer.cpp
)

set(REG_FSM_HTTP_LIBS
    Boost::json
    Boost::system
)

if(WIN32)
    list(APPEND REG_FSM_HTTP_LIBS ws2_32 mswsock)
endif()

# HTTP service executable for the manager/factory/fsm flow.
add_executable(reg_fsm_demo
    reg_fsm/main.cpp
    ${REG_FSM_CORE_SOURCES}
    ${REG_FSM_HTTP_SOURCES}
)

target_include_directories(reg_fsm_demo PRIVATE
    reg_fsm/inc
)

target_link_libraries(reg_fsm_demo PRIVATE Threads::Threads ${REG_FSM_HTTP_LIBS})

# Basic tests for message defaults, invalid transitions, manager flow, and HTTP routing.
add_executable(reg_fsm_tests
    tests/framework_tests.cpp
    ${REG_FSM_CORE_SOURCES}
    ${REG_FSM_HTTP_SOURCES}
)

target_include_directories(reg_fsm_tests PRIVATE
    reg_fsm/inc
)

target_link_libraries(reg_fsm_tests PRIVATE Threads::Threads ${REG_FSM_HTTP_LIBS})

add_test(NAME reg_fsm_tests COMMAND reg_fsm_tests)

# Benchmark executable for framework throughput, routing, concurrency, and timer baselines.
add_executable(reg_fsm_benchmark
    benchmarks/framework_benchmark.cpp
    ${REG_FSM_CORE_SOURCES}
)

target_include_directories(reg_fsm_benchmark PRIVATE
    reg_fsm/inc
)

target_link_libraries(reg_fsm_benchmark PRIVATE Threads::Threads)
