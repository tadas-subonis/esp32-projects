cmake_minimum_required(VERSION 3.16)
project(artillery_host LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Keep binaries at build-host/{name}.exe so Python tests and make.ps1 agree.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR})

option(ARTILLERY_EMU "Build desktop SDL emulator (artillery-emu)" ON)

function(artillery_warnings tgt)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W3 /utf-8)
        target_compile_definitions(${tgt} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Werror=return-type)
    endif()
endfunction()

add_library(artillery_game STATIC
    game/src/world.cpp
    game/src/physics.cpp
    game/src/match.cpp
    game/src/bot.cpp
    game/src/render.cpp
    game/src/log.cpp
)
target_include_directories(artillery_game PUBLIC game/include)
artillery_warnings(artillery_game)

add_library(artillery_authority STATIC
    server/authority.cpp
)
target_include_directories(artillery_authority PUBLIC server)
target_include_directories(artillery_authority PRIVATE shared/protocol)
target_link_libraries(artillery_authority PUBLIC artillery_game)
artillery_warnings(artillery_authority)

add_library(artillery_net STATIC
    shared/protocol/codec.cpp
    shared/protocol/wire.cpp
    shared/protocol/link.cpp
    shared/net/tcp.cpp
    shared/net/udp.cpp
)
target_include_directories(artillery_net PUBLIC shared/protocol shared/net game/include)
target_link_libraries(artillery_net PUBLIC artillery_game)
if(WIN32)
    target_link_libraries(artillery_net PUBLIC ws2_32)
endif()
artillery_warnings(artillery_net)

add_executable(artillery-sim host/sim_main.cpp)
target_link_libraries(artillery-sim PRIVATE artillery_game)
artillery_warnings(artillery-sim)

add_executable(artillery-server server/server_main.cpp)
target_link_libraries(artillery-server PRIVATE artillery_authority artillery_net)
artillery_warnings(artillery-server)

add_executable(artillery-cli host/cli_main.cpp)
target_link_libraries(artillery-cli PRIVATE artillery_net)
artillery_warnings(artillery-cli)

add_executable(artillery-host host/host_main.cpp host/proxy.cpp)
target_include_directories(artillery-host PRIVATE host)
target_link_libraries(artillery-host PRIVATE artillery_authority artillery_net)
artillery_warnings(artillery-host)

add_executable(artillery-tests game/tests/test_core.cpp)
target_link_libraries(artillery-tests PRIVATE artillery_game artillery_authority artillery_net)
artillery_warnings(artillery-tests)

add_executable(artillery-link-tests game/tests/test_link.cpp)
target_link_libraries(artillery-link-tests PRIVATE artillery_net)
artillery_warnings(artillery-link-tests)

if(ARTILLERY_EMU)
    include(FetchContent)
    cmake_policy(SET CMP0077 NEW)
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_ENABLED_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        SDL2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.11/SDL2-2.30.11.tar.gz
    )
    FetchContent_MakeAvailable(SDL2)

    add_executable(artillery-emu host/emu_main.cpp host/proxy.cpp)
    target_include_directories(artillery-emu PRIVATE host)
    target_compile_definitions(artillery-emu PRIVATE SDL_MAIN_HANDLED)
    if(TARGET SDL2::SDL2-static)
        target_link_libraries(artillery-emu PRIVATE artillery_game artillery_authority artillery_net SDL2::SDL2-static)
    elseif(TARGET SDL2-static)
        target_link_libraries(artillery-emu PRIVATE artillery_game artillery_authority artillery_net SDL2-static)
    else()
        target_link_libraries(artillery-emu PRIVATE artillery_game artillery_authority artillery_net SDL2::SDL2)
    endif()
    artillery_warnings(artillery-emu)
endif()

enable_testing()
add_test(NAME artillery-tests COMMAND artillery-tests)
add_test(NAME artillery-link-tests COMMAND artillery-link-tests)
