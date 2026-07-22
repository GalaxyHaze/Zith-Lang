cmake_minimum_required(VERSION 3.20)
project(zithc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZITH_IS_WASM "Build for WebAssembly target" OFF)

include(GNUInstallDirs)
include(FetchContent)

if(NOT ZITH_IS_WASM)
    FetchContent_Declare(
        mio
        GIT_REPOSITORY https://github.com/vimpunk/mio.git
        GIT_TAG 8b6b7d878c89e81614d05edca7936de41ccdd2da
    )
    FetchContent_MakeAvailable(mio)
endif()

FetchContent_Declare(
    frozen
    GIT_REPOSITORY https://github.com/serge-sans-paille/frozen.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(frozen)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

file(GLOB_RECURSE ZITH_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM ZITH_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/cli/zithc-main.cpp")

add_library(zithcLib STATIC ${ZITH_LIB_SOURCES})

target_include_directories(zithcLib PUBLIC src)

target_link_libraries(zithcLib PUBLIC frozen::frozen tomlplusplus::tomlplusplus)

if(NOT ZITH_IS_WASM)
    target_link_libraries(zithcLib PUBLIC mio)
endif()

target_compile_definitions(zithcLib PUBLIC ZITH_VERSION="v0.2.0-procedural")

target_compile_features(zithcLib PUBLIC cxx_std_23)

if(ZITH_IS_WASM)
    target_compile_definitions(zithcLib PUBLIC ZITH_IS_WASM=1)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(zithcLib PRIVATE
        -Wall
        -Werror
        -Wpedantic
        -Wextra
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(zithcLib PRIVATE
        -Wall
        -Werror
        -Wpedantic
        -Wextra
    )
endif()

add_executable(zithc src/cli/zithc-main.cpp)

target_link_libraries(zithc PRIVATE zithcLib)
