cmake_minimum_required(VERSION 3.20)
project(zithc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZITH_IS_WASM "Build for WebAssembly target" OFF)

include(GNUInstallDirs)
include(FetchContent)

FetchContent_Declare(
    frozen
    GIT_REPOSITORY https://github.com/serge-sans-paille/frozen.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(frozen)

add_subdirectory(src/memory)

target_compile_definitions(zith-core PUBLIC ZITH_VERSION="v0.2.0-procedural")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(zith-core PRIVATE
        -Wall
        -Werror
        -Weverything
        -Wno-unknown-warning-option
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-padded
        -Wno-missing-field-initializers
        -Wno-disabled-macro-expansion
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(zith-core PRIVATE
        -Wall
        -Werror
        -Wpedantic
        -Wextra
    )
endif()
