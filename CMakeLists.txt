cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(zithc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ZITH_VERSION OR ZITH_VERSION STREQUAL "")
    set(ZITH_VERSION "v0.0.1")
endif()
set(ZITH_VERSION "${ZITH_VERSION}" CACHE STRING "Project version" FORCE)

include(GNUInstallDirs)
include(FetchContent)

FetchContent_Declare(
    mio
    GIT_REPOSITORY https://github.com/vimpunk/mio.git
    GIT_TAG master
)
FetchContent_MakeAvailable(mio)

file(GLOB_RECURSE ZITH_SOURCES
    src/*.cpp
)

add_executable(zithc ${ZITH_SOURCES})

target_include_directories(zithc PRIVATE
    src
    include
)

target_link_libraries(zithc PRIVATE mio)

target_compile_definitions(zithc PRIVATE
    ZITH_VERSION="${ZITH_VERSION}"
    ZITH_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/lib"
)

file(GLOB_RECURSE ZITH_TEST_SOURCES tests/unit/*.cpp tests/integration/*.cpp)

add_executable(zithc-test ${ZITH_TEST_SOURCES})
target_include_directories(zithc-test PRIVATE src include)
target_link_libraries(zithc-test PRIVATE mio)

install(TARGETS zithc
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY lib/
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/zith/lib
    FILES_MATCHING PATTERN "*.zith"
)

find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-18 clang-format-17 clang-format-16)

if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ZITH_FORMAT_SOURCES
        "src/*.cpp" "src/*.hpp" "src/*.h" "src/*.c"
        "include/*.hpp" "include/*.h"
        "tests/*.cpp" "tests/*.h"
    )

    add_custom_target(fmt
        COMMAND ${CLANG_FORMAT_EXE} -i ${ZITH_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    add_custom_target(fmt-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${ZITH_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
