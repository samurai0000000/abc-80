#
# CMakeLists.txt
#
# Copyright (C) 2026, Charles Chiou
#

cmake_minimum_required(VERSION 3.16)
project(abc80 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" ON)

if(ENABLE_ASAN)
    message(STATUS "Enabling AddressSanitizer and UndefinedBehaviorSanitizer (-fsanitize=address,undefined)")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/chips
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sokol
)

# Third-party: CppUTest
set(CPPUTEST_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CPPUTEST_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CPPUTEST_MEM_LEAK_DETECTION_DISABLED ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/cpputest EXCLUDE_FROM_ALL)

# Core library
add_library(abc80_core STATIC
    src/Abc80Bus.cxx
    src/Abc80Ppi.cxx
    src/Abc80Cpu.cxx
    src/Abc80Board.cxx
    src/Abc80Rom.cxx
    src/KeyQueue.cxx
    src/WebProtocol.cxx
    src/Abc80WebServer.cxx
    src/ServerRunner.cxx
    src/Abc80Tape.cxx
    src/Abc80Cli.cxx
    src/Abc80QbHost.cxx
)
find_package(Threads REQUIRED)
target_include_directories(abc80_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/chips
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sokol
)
# Third-party headers are SYSTEM so -Wall -Wextra -Werror -pedantic apply only to our code.
target_include_directories(abc80_core SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
)
target_link_libraries(abc80_core PUBLIC Threads::Threads)
# One WebSocket close handshake wait for every translation unit (the library default is 5 s, which
# would delay shutdown behind an idle client). Defined globally so all inline definitions agree.
target_compile_definitions(abc80_core PUBLIC CPPHTTPLIB_WEBSOCKET_CLOSE_TIMEOUT_SECOND=1)

# CLI Standalone Executable
add_executable(abc80_cli
    src/Main.cxx
)
target_link_libraries(abc80_cli PRIVATE
    abc80_core
)

# Web front panel server
add_executable(abc80_server
    src/ServerMain.cxx
)
target_link_libraries(abc80_server PRIVATE
    abc80_core
)

# Test executable
enable_testing()
add_executable(abc80_tests
    tests/Main.cxx
    tests/Abc80BusTest.cxx
    tests/Abc80PpiTest.cxx
    tests/Abc80CpuTest.cxx
    tests/Abc80CpuOpcodeTest.cxx
    tests/Abc80FaultInjectionTest.cxx
    tests/Abc80ZexdocTest.cxx
    tests/Abc80RomSubroutineTest.cxx
    tests/Abc80BoardTest.cxx
    tests/Abc80TapeTest.cxx
    tests/Abc80CliTest.cxx
    tests/Abc80QbHostTest.cxx
    tests/Abc80RomCharacterizationTest.cxx
    tests/Abc80DisplayTest.cxx
    tests/Abc80SpeakerTest.cxx
    tests/Abc80KeypadTest.cxx
    tests/Abc80KeyQueueTest.cxx
    tests/Abc80LedTest.cxx
    tests/Abc80ResetTest.cxx
    tests/Abc80WebProtocolTest.cxx
    tests/Abc80WebServerTest.cxx
    tests/Abc80ServerArgsTest.cxx
)
# Property tests need RapidCheck, installed system-wide (as aimon uses it).
find_library(RAPIDCHECK_LIBRARY rapidcheck)
if(NOT RAPIDCHECK_LIBRARY)
    message(FATAL_ERROR "RapidCheck library not found (needed by the key queue property tests)")
endif()
target_link_libraries(abc80_tests PRIVATE
    abc80_core
    CppUTest
    CppUTestExt
    ${RAPIDCHECK_LIBRARY}
)
add_test(NAME abc80_tests COMMAND abc80_tests)
