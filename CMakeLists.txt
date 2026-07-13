cmake_minimum_required(VERSION 3.20)
project(zithc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ZITH_BUILD_CLI "Build the zithc CLI executable" ON)
option(ZITH_BUILD_CAPI "Build the C ABI library" ON)
option(ZITH_IS_WASM "Build for WebAssembly target" OFF)
option(ZITH_HAS_LLVM "Enable LLVM codegen backend" ON)

set(ZITH_VERSION "v0.0.1" CACHE STRING "Project version")

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
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

add_subdirectory(src/memory)

file(GLOB_RECURSE ZITH_LIB_SOURCES src/*.cpp)
list(REMOVE_ITEM ZITH_LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/cli/zithc-main.cpp")

if(NOT ZITH_HAS_LLVM OR ZITH_IS_WASM)
    file(GLOB_RECURSE CODEGEN_SOURCES src/codegen/*.cpp)
    list(REMOVE_ITEM ZITH_LIB_SOURCES ${CODEGEN_SOURCES})
endif()

add_library(zithcLib STATIC ${ZITH_LIB_SOURCES})

target_include_directories(zithcLib PUBLIC src)

if(ZITH_IS_WASM)
    target_compile_definitions(zithcLib PUBLIC ZITH_IS_WASM=1)
    target_compile_definitions(zithcLib PRIVATE TOML_EXCEPTIONS=0)
endif()

target_link_libraries(zithcLib PUBLIC
    zith-core
    tomlplusplus::tomlplusplus
)

if(NOT ZITH_IS_WASM)
    target_link_libraries(zithcLib PUBLIC mio)
    get_target_property(_mio_includes mio INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(zithcLib SYSTEM PUBLIC ${_mio_includes})
endif()

if(ZITH_HAS_LLVM AND NOT ZITH_IS_WASM)
    find_package(LLVM QUIET CONFIG)
    if(LLVM_FOUND)
        message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
        message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

        llvm_map_components_to_libnames(LLVM_LIBS core support irreader passes target X86CodeGen X86AsmParser X86Desc X86Info MC MCDisassembler MCParser AsmPrinter SelectionDAG GlobalISel)

        target_link_libraries(zithcLib PUBLIC ${LLVM_LIBS})
        target_include_directories(zithcLib SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
        target_compile_definitions(zithcLib PUBLIC ZITH_HAS_LLVM=1)
    else()
        message(WARNING "LLVM not found — disabling codegen backend")
        set(ZITH_HAS_LLVM OFF CACHE BOOL "" FORCE)
    endif()
endif()

target_compile_definitions(zithcLib PUBLIC ZITH_VERSION="${ZITH_VERSION}")

if(ZITH_BUILD_CLI AND NOT ZITH_IS_WASM)
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

if(ZITH_BUILD_CLI AND NOT ZITH_IS_WASM)
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

# ── Compiler flags: -Werror -Weverything for Clang ────────────────────────

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(ZITH_CLANG_WARNINGS
        -Wall
        -Werror
        -Weverything
        -Wno-switch-enum
        -Wno-c++98-compat
        -Wno-switch-default
        -Wno-c++98-compat-pedantic
        -Wno-reserved-module-identifier
        -Wno-unsafe-buffer-usage
        -Wno-padded
        -Wno-unsafe-buffer-usage-in-libc-call
        -Wno-old-style-cast
        -Wno-format-nonliteral
        -Wno-missing-field-initializers
    )
    target_compile_options(zithcLib PRIVATE ${ZITH_CLANG_WARNINGS})
    if(TARGET zithc)
        target_compile_options(zithc PRIVATE ${ZITH_CLANG_WARNINGS})
    endif()
    if(BUILD_TESTING)
        foreach(TARGET test-lexer test-import test-alias test-struct test-scan)
            if(TARGET ${TARGET})
                target_compile_options(${TARGET} PRIVATE ${ZITH_CLANG_WARNINGS})
            endif()
        endforeach()
    endif()
endif()
