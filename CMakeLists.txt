cmake_minimum_required(VERSION 3.10)

project(FsmFramework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
enable_testing()

# Demo executable for the full manager/factory/fsm registration flow.
add_executable(reg_fsm_demo
    reg_fsm/main.cpp
    reg_fsm/src/AuthFactory.cpp
    reg_fsm/src/AuthFsm.cpp
    reg_fsm/src/CMsg.cpp
    reg_fsm/src/Cfsm.cpp
    reg_fsm/src/Cfactory.cpp
    reg_fsm/src/Cfactory_mgr.cpp
    reg_fsm/src/RegFactory.cpp
    reg_fsm/src/RegFsm.cpp
    reg_fsm/src/TimerManager.cpp
)

target_include_directories(reg_fsm_demo PRIVATE
    reg_fsm/inc
)

target_link_libraries(reg_fsm_demo PRIVATE Threads::Threads)

# Basic tests for message defaults, invalid transitions, and the main flow.
add_executable(reg_fsm_tests
    tests/framework_tests.cpp
    reg_fsm/src/AuthFactory.cpp
    reg_fsm/src/AuthFsm.cpp
    reg_fsm/src/CMsg.cpp
    reg_fsm/src/Cfsm.cpp
    reg_fsm/src/Cfactory.cpp
    reg_fsm/src/Cfactory_mgr.cpp
    reg_fsm/src/RegFactory.cpp
    reg_fsm/src/RegFsm.cpp
    reg_fsm/src/TimerManager.cpp
)

target_include_directories(reg_fsm_tests PRIVATE
    reg_fsm/inc
)

target_link_libraries(reg_fsm_tests PRIVATE Threads::Threads)

add_test(NAME reg_fsm_tests COMMAND reg_fsm_tests)
