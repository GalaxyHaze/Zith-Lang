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
file(GLOB_RECURSE ZITH_LIB_SOURCES
    src/*.cpp
)
list(REMOVE_ITEM ZITH_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/cli/zithc-main.cpp")

add_library(zithc_objects OBJECT ${ZITH_LIB_SOURCES})

target_include_directories(zithc_objects PRIVATE
    src
    include
)

target_link_libraries(zithc_objects PRIVATE mio)

target_compile_definitions(zithc_objects PRIVATE
    ZITH_VERSION="${ZITH_VERSION}"
    ZITH_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/lib"
)

add_executable(zithc ${CMAKE_SOURCE_DIR}/src/cli/zithc-main.cpp $<TARGET_OBJECTS:zithc_objects>)

target_include_directories(zithc PRIVATE
    src
    include
)

target_link_libraries(zithc PRIVATE mio)

target_compile_definitions(zithc PRIVATE
    ZITH_VERSION="${ZITH_VERSION}"
    ZITH_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/lib"
)

include(CTest)

if(BUILD_TESTING)

function(add_zith_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE} $<TARGET_OBJECTS:zithc_objects>)
    target_include_directories(${NAME} PRIVATE src include)
    target_link_libraries(${NAME} PRIVATE mio)
    set_target_properties(${NAME} PROPERTIES FOLDER "tests")
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

add_zith_test(zith-lexer-test  tests/unit/lexer-tokens.cpp)
add_zith_test(zith-parser-expr  tests/unit/parser-expr.cpp)
add_zith_test(zith-parser-stmt  tests/unit/parser-stmt.cpp)
add_zith_test(zith-sema-test    tests/unit/sema-basics.cpp)
add_zith_test(zith-mir-test     tests/unit/mir-lowering.cpp)

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
