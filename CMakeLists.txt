cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.20)
project(zithc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ZITH_VERSION OR ZITH_VERSION STREQUAL "")
    execute_process(
        COMMAND git ls-remote --tags https://github.com/GalaxyHaze/Zith.git
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAGS
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GIT_TAGS)
        string(REGEX MATCHALL "refs/tags/v[0-9]+\\.[0-9]+\\.[0-9]+" VERSION_TAGS "${GIT_TAGS}")
        if(VERSION_TAGS)
            list(SORT VERSION_TAGS COMPARE NATURAL)
            list(POP_BACK VERSION_TAGS LATEST_TAG)
            string(REGEX REPLACE "refs/tags/(.*)" "\\1" ZITH_VERSION "${LATEST_TAG}")
        endif()
    endif()
    if(NOT DEFINED ZITH_VERSION OR ZITH_VERSION STREQUAL "")
        set(ZITH_VERSION "v0.0.1")
    endif()
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

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG master
)
FetchContent_MakeAvailable(tomlplusplus)

file(GLOB_RECURSE ZITH_SOURCES
    src/*.cpp
)
file(GLOB_RECURSE ZITH_LIB_SOURCES
    src/*.cpp
)
list(REMOVE_ITEM ZITH_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/cli/zithc-main.cpp")

add_library(zithc_objects OBJECT ${ZITH_LIB_SOURCES})

target_include_directories(zithc_objects PRIVATE
    src
)

target_link_libraries(zithc_objects PRIVATE mio tomlplusplus::tomlplusplus)

target_compile_definitions(zithc_objects PRIVATE
    ZITH_VERSION="${ZITH_VERSION}"
    ZITH_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/lib"
)

add_executable(zithc ${CMAKE_SOURCE_DIR}/src/cli/zithc-main.cpp $<TARGET_OBJECTS:zithc_objects>)

target_include_directories(zithc PRIVATE
    src
)

target_link_libraries(zithc PRIVATE mio tomlplusplus::tomlplusplus)

target_compile_definitions(zithc PRIVATE
    ZITH_VERSION="${ZITH_VERSION}"
    ZITH_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/lib"
)

include(CTest)

if(BUILD_TESTING)

function(add_zith_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE} $<TARGET_OBJECTS:zithc_objects>)
    target_include_directories(${NAME} PRIVATE src)
    target_link_libraries(${NAME} PRIVATE mio tomlplusplus::tomlplusplus)
    set_target_properties(${NAME} PROPERTIES FOLDER "tests")
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

add_zith_test(zithc-lexer-test  tests/unit/lexer-tokens.cpp)
add_zith_test(zithc-parser-expr  tests/unit/parser-expr.cpp)
add_zith_test(zithc-parser-stmt  tests/unit/parser-stmt.cpp)
add_zith_test(zithc-sema-test    tests/unit/sema-basics.cpp)
add_zith_test(zithc-mir-test     tests/unit/mir-lowering.cpp)
add_zith_test(zithc-import-test  tests/unit/import-basics.cpp)
add_zith_test(zithc-import-e2e   tests/unit/import-e2e.cpp)

endif()

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
