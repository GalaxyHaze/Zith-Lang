cmake_minimum_required(VERSION 3.20)
project(zithc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ZITH_BUILD_CLI "Build the zithc CLI executable" ON)
option(ZITH_BUILD_CAPI "Build the C ABI library" ON)
option(ZITH_IS_WASM "Build for WebAssembly target" OFF)

set(ZITH_VERSION "v0.0.1" CACHE STRING "Project version")

include(GNUInstallDirs)
include(FetchContent)

if(NOT ZITH_IS_WASM)
    FetchContent_Declare(
        mio
        GIT_REPOSITORY https://github.com/vimpunk/mio.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(mio)
endif()

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

add_subdirectory(src/memory)

file(GLOB_RECURSE ZITH_LIB_SOURCES src/*.cpp)
list(REMOVE_ITEM ZITH_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/cli/zithc-main.cpp")

add_library(zithcLib STATIC ${ZITH_LIB_SOURCES})

target_include_directories(zithcLib PUBLIC src)

if(ZITH_IS_WASM)
    target_compile_definitions(zithcLib PUBLIC ZITH_IS_WASM=1)
    target_compile_definitions(tomlplusplus::tomlplusplus INTERFACE TOML_EXCEPTIONS=0)
endif()

target_link_libraries(zithcLib PUBLIC
    zith-core
    tomlplusplus::tomlplusplus
)

if(NOT ZITH_IS_WASM)
    target_link_libraries(zithcLib PUBLIC mio)
endif()

target_compile_definitions(zithcLib PUBLIC ZITH_VERSION="${ZITH_VERSION}")

if(ZITH_BUILD_CLI)
    add_executable(zithc src/cli/zithc-main.cpp)
    target_link_libraries(zithc PRIVATE zithcLib)
endif()

include(CTest)

if(BUILD_TESTING)
    function(add_zith_test NAME SOURCE)
        add_executable(${NAME} ${SOURCE})
        target_link_libraries(${NAME} PRIVATE zithcLib)
        add_test(NAME ${NAME} COMMAND ${NAME})
    endfunction()

    add_zith_test(test-lexer tests/test-lexer.cpp)
    add_zith_test(test-import tests/test-import.cpp)
    add_zith_test(test-alias tests/test-alias.cpp)
    add_zith_test(test-struct tests/test-struct.cpp)
    add_zith_test(test-scan tests/test-scan.cpp)
endif()

if(ZITH_BUILD_CLI)
    install(TARGETS zithc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(ZITH_BUILD_CAPI)
    install(TARGETS zithcLib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/zithc
    )
endif()

install(DIRECTORY stdlib/
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/zith/stdlib
    FILES_MATCHING PATTERN "*.zith"
)

find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-18 clang-format-17 clang-format-16)

if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ZITH_FORMAT_SOURCES
        "src/*.cpp" "src/*.hpp" "src/*.h" "src/*.c"
        "tests/*.cpp" "tests/*.h"
    )
    add_custom_target(fmt COMMAND ${CLANG_FORMAT_EXE} -i ${ZITH_FORMAT_SOURCES})
    add_custom_target(fmt-check COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${ZITH_FORMAT_SOURCES})
endif()
