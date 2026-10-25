#
# CMakeLists.txt
#
# Copyright (C) 2026, Charles Chiou
#

cmake_minimum_required(VERSION 3.16)

# Extract version from root README.md ("**Version: major.minor.patch**")
set(_ROOT_README_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ROOT_README_FILE}")
file(READ "${_ROOT_README_FILE}" _ROOT_README_CONTENT)
string(REGEX MATCH "\\*\\*Version:[ \t]*([0-9]+)\\.([0-9]+)\\.([0-9]+)\\*\\*" _VER_MATCH "${_ROOT_README_CONTENT}")
if(NOT _VER_MATCH)
    message(FATAL_ERROR "Could not extract **Version: x.y.z** from ${_ROOT_README_FILE}")
endif()
set(PROJ_VER "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")

project(abc80 VERSION ${PROJ_VER} LANGUAGES C CXX)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.hxx.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/abc80/Version.hxx"
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

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

# Third-party libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(CONFIGPP REQUIRED libconfig++)

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
    src/Abc80Config.cxx
    src/AiKeySequencer.cxx
    src/AimonGatewayClient.cxx
    src/ServerRunner.cxx
    src/Abc80Tape.cxx
    src/Abc80Cli.cxx
    src/Abc80QbHost.cxx
    src/Abc80ExtCircuit.cxx
)
find_package(Threads REQUIRED)
target_include_directories(abc80_core PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/chips
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sokol
    ${CONFIGPP_INCLUDE_DIRS}
)
# Third-party headers are SYSTEM so -Wall -Wextra -Werror -pedantic apply only to our code.
target_include_directories(abc80_core SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
)
target_link_libraries(abc80_core PUBLIC
    Threads::Threads
    ${CONFIGPP_LIBRARIES}
)
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

if(UNIX AND NOT APPLE AND NOT ENABLE_ASAN)
    set(ABC80_DEB_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/abc-80_${PROJECT_VERSION}_amd64.deb")
    add_custom_command(
        OUTPUT "${ABC80_DEB_OUTPUT}"
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/packaging/build_deb.sh"
                "$<TARGET_FILE:abc80_server>"
                "$<TARGET_FILE:abc80_cli>"
                "${PROJECT_VERSION}"
                "${CMAKE_CURRENT_BINARY_DIR}"
                "amd64"
        DEPENDS abc80_server
                abc80_cli
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/build_deb.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/abc-80.service"
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/abc-80.default"
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/abc-80.cfg"
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/conffiles"
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/postinst"
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/prerm"
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/postrm"
                "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
                "${CMAKE_CURRENT_SOURCE_DIR}/web/app.js"
                "${CMAKE_CURRENT_SOURCE_DIR}/web/style.css"
        COMMENT "Generating Debian package: ${ABC80_DEB_OUTPUT}"
        VERBATIM
    )
    add_custom_target(abc80_deb ALL DEPENDS "${ABC80_DEB_OUTPUT}")
endif()

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
    tests/Abc80EpromProgrammerTest.cxx
    tests/Abc80BookRomErrataTest.cxx
    tests/Abc80BookRomGapTest.cxx
    tests/Abc80ResetTest.cxx
    tests/Abc80WebProtocolTest.cxx
    tests/Abc80WebServerTest.cxx
    tests/Abc80ServerArgsTest.cxx
    tests/Abc80ConfigTest.cxx
    tests/Abc80AiKeySequencerTest.cxx
    tests/Abc80ExercisesTest.cxx
    tests/Abc80ExtCircuitTest.cxx
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
add_dependencies(abc80_tests abc80_server abc80_cli)
if(TARGET abc80_deb)
    add_dependencies(abc80_tests abc80_deb)
endif()
if(ENABLE_ASAN)
    set(_DEB_LOOKUP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build/abc-80_${PROJECT_VERSION}_amd64.deb")
else()
    set(_DEB_LOOKUP_PATH "${CMAKE_CURRENT_BINARY_DIR}/abc-80_${PROJECT_VERSION}_amd64.deb")
endif()
target_compile_definitions(abc80_tests PRIVATE
    ABC80_SERVER_BINARY="$<TARGET_FILE:abc80_server>"
    ABC80_CLI_BINARY="$<TARGET_FILE:abc80_cli>"
    ABC80_DEB_PATH="${_DEB_LOOKUP_PATH}"
)
add_test(NAME abc80_tests COMMAND abc80_tests)
