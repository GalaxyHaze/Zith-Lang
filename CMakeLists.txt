cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.20)
project(zithc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ZITH_BUILD_CLI  "Build the zithc CLI executable"  ON)
option(ZITH_BUILD_CAPI "Build the C ABI (zithcLib library)" ON)
option(ZITH_IS_WASM    "Build for WebAssembly target"    OFF)

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
    GIT_TAG master
)
FetchContent_MakeAvailable(tomlplusplus)

file(GLOB_RECURSE ZITH_SOURCES
    src/*.cpp
)

# Compiler library sources (all .cpp minus main entry point)
file(GLOB_RECURSE ZITH_LIB_SOURCES
    src/*.cpp
)
list(REMOVE_ITEM ZITH_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/cli/zithc-main.cpp")

# ── Static library (all sources except main entry point) ──────────
add_library(zithcLib STATIC ${ZITH_LIB_SOURCES})

target_include_directories(zithcLib PUBLIC
    src
)

if(ZITH_IS_WASM)
    target_compile_definitions(zithcLib PUBLIC ZITH_IS_WASM=1)
else()
    target_link_libraries(zithcLib PUBLIC mio)
endif()

target_link_libraries(zithcLib PUBLIC tomlplusplus::tomlplusplus)

target_compile_definitions(zithcLib PUBLIC
    ZITH_VERSION="${ZITH_VERSION}"
)

if(ZITH_IS_WASM)
    target_compile_definitions(tomlplusplus::tomlplusplus INTERFACE TOML_EXCEPTIONS=0)
endif()

set_target_properties(zithcLib PROPERTIES
    OUTPUT_NAME "zithc"
    PUBLIC_HEADER "src/capi/zithc-capi.h"
)

if(ZITH_BUILD_CLI)
    # ── CLI executable ────────────────────────────────────────────────
    add_executable(zithc ${CMAKE_SOURCE_DIR}/src/cli/zithc-main.cpp)
    target_link_libraries(zithc PRIVATE zithcLib)
endif()

include(CTest)

if(BUILD_TESTING)

function(add_zith_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    target_link_libraries(${NAME} PRIVATE zithcLib)
    set_target_properties(${NAME} PROPERTIES FOLDER "tests")
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

add_zith_test(zithc-lexer-test     tests/unit/lexer-tokens.cpp)
add_zith_test(zithc-parser-expr    tests/unit/parser-expr.cpp)
add_zith_test(zithc-parser-stmt    tests/unit/parser-stmt.cpp)
add_zith_test(zithc-parser-scan    tests/unit/parser-scan.cpp)
add_zith_test(zithc-parser-full    tests/unit/parser-full.cpp)
add_zith_test(zithc-sema-test      tests/unit/sema-basics.cpp)
add_zith_test(zithc-type-dedup     tests/unit/type-dedup.cpp)
add_zith_test(zithc-unify-errors   tests/unit/unify-errors.cpp)
add_zith_test(zithc-sema-hir       tests/unit/sema-hir.cpp)
add_zith_test(zithc-mir-test       tests/unit/mir-lowering.cpp)
add_zith_test(zithc-import-test    tests/unit/import-basics.cpp)
add_zith_test(zithc-import-e2e     tests/unit/import-e2e.cpp)
add_zith_test(zithc-heuristic-engine tests/unit/heuristic-engine.cpp)
add_zith_test(zithc-ast-printer      tests/unit/ast-printer.cpp)
add_zith_test(zithc-resolver-test    tests/unit/resolver-test.cpp)
add_zith_test(zithc-arena-test       tests/unit/arena-test.cpp)
add_zith_test(zithc-source-map-test tests/unit/source-map-test.cpp)
add_zith_test(zithc-platform-test   tests/unit/platform-test.cpp)
add_zith_test(zithc-error-codes     tests/unit/error-codes-lookup.cpp)
add_zith_test(zithc-diag-advanced   tests/unit/diagnostic-engine-advanced.cpp)
add_zith_test(zithc-result          tests/unit/result-test.cpp)
add_zith_test(zithc-string-interner tests/unit/string-interner-test.cpp)
add_zith_test(zithc-dyn-array-test tests/unit/dyn-array-test.cpp)
add_zith_test(zithc-flat-map-test tests/unit/flat-map-test.cpp)
add_zith_test(zithc-fmt-visitor      tests/unit/fmt-visitor.cpp)
add_zith_test(zithc-zir-inst       tests/unit/zir/test_zir_inst.cpp)
add_zith_test(zithc-zir-emitter    tests/unit/zir/test_zir_emitter.cpp)
add_zith_test(zithc-zir-interp     tests/unit/zir/test_zir_interp.cpp)

endif()

if(ZITH_BUILD_CLI)
    install(TARGETS zithc
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
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

    add_custom_target(fmt
        COMMAND ${CLANG_FORMAT_EXE} -i ${ZITH_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    add_custom_target(fmt-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${ZITH_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
