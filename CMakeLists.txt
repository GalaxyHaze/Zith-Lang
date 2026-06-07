cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(zith LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# Project Version
# =============================================================================
if(NOT DEFINED ZITH_VERSION OR ZITH_VERSION STREQUAL "")
    set(ZITH_VERSION "v0.0.1")
endif()
set(ZITH_VERSION "${ZITH_VERSION}" CACHE STRING "Project version" FORCE)

# =============================================================================
# External Dependencies
# =============================================================================
include(FetchContent)

message(STATUS "Fetching TOML++...")
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tomlplusplus)
message(STATUS "TOML++ available")

# =============================================================================
# Compiler Flags & Warnings
# =============================================================================
option(ENABLE_WERROR "Treat warnings as errors" OFF)

if (MSVC)
    add_compile_options(/W4 /wd4100 /wd4201)
    if(ENABLE_WERROR)
        add_compile_options(/WX)
    endif()
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(ENABLE_WERROR)
        add_compile_options(-Werror)
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        find_program(LLD_EXE NAMES ld.lld-18 ld.lld-17 ld.lld-16 ld.lld lld)
        if(LLD_EXE)
            get_filename_component(LLD_NAME "${LLD_EXE}" NAME_WE)
            if (NOT CMAKE_EXE_LINKER_FLAGS MATCHES "-fuse-ld=")
                set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=${LLD_NAME}")
                set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=${LLD_NAME}")
            endif()
        endif()
    endif ()
endif ()

# =============================================================================
# Include Directories
# =============================================================================
set(ZITH_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/impl
)

# =============================================================================
# Source Files
# =============================================================================
set(CORE_C_SOURCES
    impl/memory/arena_c_functions.c
    impl/memory/file.c
)

set(DIAGNOSTICS_SOURCES
    impl/diagnostics/diagnostics.cpp
    impl/diagnostics/diagnostic-bag.cpp
    impl/diagnostics/builder.cpp
    impl/diagnostics/span.cpp
    impl/diagnostics/emitter.cpp
    impl/diagnostics/heuristic-engine.cpp
)

set(PARSER_CPP_SOURCES
    impl/ast/ast.cpp
    impl/lexer/keywords.cpp
    impl/lexer/tokenizer.cpp
    impl/parser/parser.cpp
    impl/parser/parser_decl.cpp
    impl/parser/parser_expr.cpp
    impl/parser/parser_sema.cpp
    impl/parser/parser_utils.cpp
    impl/parser/type-checker.cpp
    impl/cli/project_config/project-config.cpp
)

set(CLI_SOURCES
    impl/cli/main.cpp
    impl/cli/pipeline/pipeline.cpp
    impl/cli/runtime_interpreted/runtime-interpreted.cpp
    impl/cli/cmd/build/build.cpp
    impl/cli/cmd/run/run.cpp
    impl/cli/cmd/project/project.cpp
    impl/cli/cmd/tool/tool.cpp
    impl/cli/cmd/interactive/interactive.cpp
    impl/cli/cmd/info/info.cpp
)

# =============================================================================
# Library: ZithCore
# =============================================================================
add_library(ZithCore STATIC ${CORE_C_SOURCES} ${DIAGNOSTICS_SOURCES})

target_include_directories(ZithCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/impl>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(ZithCore PUBLIC tomlplusplus::tomlplusplus)

target_compile_definitions(ZithCore PRIVATE
    ZITH_VERSION="${ZITH_VERSION}"
    ZITH_ALLOW_STD=1
    ZITH_ALLOW_UTILS=1
    ZITH_ALLOW_C=1
)

# =============================================================================
# Library: ZithParse
# =============================================================================
add_library(ZithParse STATIC ${PARSER_CPP_SOURCES})

target_include_directories(ZithParse
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/impl>
)

target_link_libraries(ZithParse PUBLIC ZithCore)

target_compile_definitions(ZithParse PRIVATE
    ZITH_VERSION="${ZITH_VERSION}"
    ZITH_LIB_DIR="${CMAKE_CURRENT_SOURCE_DIR}/lib"
    ZITH_ALLOW_STD=1
    ZITH_ALLOW_UTILS=1
    ZITH_ALLOW_C=1
)

set_target_properties(ZithParse PROPERTIES
    VERSION 0.0.1
    SOVERSION 0
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# =============================================================================
# Executable: zith
# =============================================================================
add_executable(zith ${CLI_SOURCES})

target_include_directories(zith PRIVATE ${ZITH_INCLUDE_DIRS})
target_link_libraries(zith PRIVATE ZithParse)

target_compile_definitions(zith PRIVATE
    ZITH_VERSION="${ZITH_VERSION}"
    ZITH_EXECUTABLE
)

# =============================================================================
# Tests
# =============================================================================
include(CTest)

if(BUILD_TESTING)
    # Test helper: test.h
    set(TEST_INCLUDE_DIRS ${ZITH_INCLUDE_DIRS})

    # Test: diagnostics
    add_executable(test-diag tests/test_diagnostics_new.cpp)
    target_include_directories(test-diag PRIVATE ${TEST_INCLUDE_DIRS})
    target_link_libraries(test-diag PRIVATE ZithCore)
    add_test(NAME test-diag COMMAND test-diag)

    # Test: scan (parser)
    add_executable(test-scan tests/test_parser.cpp impl/parser/parser_test.cpp)
    target_include_directories(test-scan PRIVATE ${TEST_INCLUDE_DIRS})
    target_link_libraries(test-scan PRIVATE ZithParse)
    add_test(NAME test-scan COMMAND test-scan)

    # Test: full (expand + sema)
    add_executable(test-full tests/test_expand_sema.cpp impl/parser/parser_test.cpp)
    target_include_directories(test-full PRIVATE ${TEST_INCLUDE_DIRS})
    target_link_libraries(test-full PRIVATE ZithParse)
    add_test(NAME test-full COMMAND test-full)

    # Test: import
    add_executable(test-import tests/test_import.cpp impl/parser/parser_test.cpp)
    target_include_directories(test-import PRIVATE ${TEST_INCLUDE_DIRS})
    target_link_libraries(test-import PRIVATE ZithParse)
    add_test(NAME test-import COMMAND test-import)

    # Test: module (import system)
    add_executable(test-module tests/test_import_system.cpp)
    target_include_directories(test-module PRIVATE ${TEST_INCLUDE_DIRS})
    target_link_libraries(test-module PRIVATE ZithParse)
    add_test(NAME test-module COMMAND test-module)

    # Test: config
    add_executable(test-config tests/test_project_config.cpp)
    target_include_directories(test-config PRIVATE ${TEST_INCLUDE_DIRS})
    target_link_libraries(test-config PRIVATE ZithParse)
    add_test(NAME test-config COMMAND test-config)
endif()

# =============================================================================
# Install
# =============================================================================
install(TARGETS zith ZithParse ZithCore
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY lib/
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/zith/lib
    FILES_MATCHING PATTERN "*.zith"
)

# =============================================================================
# Formatting
# =============================================================================
find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-18 clang-format-17 clang-format-16)

if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ZITH_FORMAT_SOURCES
        "impl/*.cpp" "impl/*.hpp" "impl/*.h" "impl/*.c"
        "include/*.hpp" "include/*.h"
        "tests/*.cpp" "tests/*.hF"
    )

    add_custom_target(fmt
        COMMAND ${CLANG_FORMAT_EXE} -i ${ZITH_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting source files with clang-format"
    )

    add_custom_target(fmt-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${ZITH_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking code formatting with clang-format"
    )

    message(STATUS "Code formatting targets: 'fmt' and 'fmt-check'")
else()
    message(STATUS "clang-format not found")
endif()
