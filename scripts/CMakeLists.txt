cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(zith LANGUAGES C CXX)

set(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

include(GNUInstallDirs)

# =============================================================================
# Standards & Policies
# =============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif ()

# =============================================================================
# Output Directories
# =============================================================================
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# Project Version (Dynamic via CI or Default v0.0.1)
# =============================================================================
if(NOT DEFINED ZITH_VERSION OR ZITH_VERSION STREQUAL "")
    set(ZITH_VERSION "v0.0.1")
endif()
set(ZITH_VERSION "${ZITH_VERSION}" CACHE STRING "Project version" FORCE)

# =============================================================================
# Initialize Dependency Variables
# =============================================================================
set(LLVM_FOUND FALSE)
set(ZITH_HAS_FFI OFF)
set(ZITH_HAS_PKGCONFIG OFF)

# =============================================================================
# Fetch External Dependencies
# =============================================================================
include(FetchContent)

# ───────────────────────────────────────────────
# CLI11 - Build as static library to fix linker errors
# ───────────────────────────────────────────────
message(STATUS "Fetching CLI11...")

# Force CLI11 to build as a static library (CLI11_PRECOMPILED).
# This compiles the .cpp files required for vtables that are missing in header-only mode.
set(CLI11_PRECOMPILED ON CACHE BOOL "" FORCE)
set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.6.2
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(cli11)

# CLI11::CLI11 target is now an actual static library object, not just headers
message(STATUS "CLI11 fetched and available (Precompiled Static Lib)")

# ───────────────────────────────────────────────
# TOML++ - Header-Only Library
# ───────────────────────────────────────────────
message(STATUS "Fetching TOML++...")
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(tomlplusplus)
message(STATUS "TOML++ available")

# ───────────────────────────────────────────────
# nlohmann/json - Header-Only Library (v3.10.5 for CMake 3.15 compatibility)
# ───────────────────────────────────────────────
message(STATUS "Fetching nlohmann/json...")
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.10.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)
message(STATUS "nlohmann/json available")

# =============================================================================
# LLVM Configuration (Optional) - Must come BEFORE FFI setup
# =============================================================================
option(ZITH_ENABLE_LLVM "Enable LLVM backend support" ON)
set(LLVM_DIR "" CACHE PATH "Path to LLVMConfig.cmake (optional)")

if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    message(STATUS "Building for macOS architecture: ${CMAKE_OSX_ARCHITECTURES}")
endif()

if(ZITH_ENABLE_LLVM)
    message(STATUS "Searching for LLVM...")
    
    if(LLVM_DIR)
        message(STATUS "   Using specified LLVM_DIR: ${LLVM_DIR}")
        find_package(LLVM QUIET CONFIG PATHS "${LLVM_DIR}" NO_DEFAULT_PATH)
    else()
        message(STATUS "   Searching system paths for LLVM...")
        find_package(LLVM QUIET CONFIG)
    endif()

    if(LLVM_FOUND)
        message(STATUS "LLVM found: version ${LLVM_PACKAGE_VERSION}")
        message(STATUS "   LLVM directory: ${LLVM_DIR}")
        message(STATUS "   LLVM include path: ${LLVM_INCLUDE_DIRS}")

        # Define LLVM version macro
        add_compile_definitions(LLVM_VERSION_MAJOR=${LLVM_VERSION_MAJOR})
        
        # Map LLVM components to actual libraries (static linking)
        llvm_map_components_to_libnames(LLVM_LIBRARIES
            analysis
            core
            instcombine
            scalaropts
            vectorize
            ipo
            irreader
            support
            target
        )

        if(LLVM_LIBRARIES)
            message(STATUS "   LLVM libraries mapped successfully")
        else()
            message(WARNING "LLVM found but no libraries could be mapped")
            set(LLVM_FOUND FALSE)
        endif()

        # ───────────────────────────────────────────────
        # FFI Setup - Only if LLVM is available
        # ───────────────────────────────────────────────
        if(NOT WIN32 AND NOT EMSCRIPTEN)
            option(ZITH_ENABLE_FFI "Enable FFI support via libffi (requires libffi-dev installed)" ON)
            
            if(ZITH_ENABLE_FFI)
                message(STATUS "Searching for libffi...")
                
                # Check if pkg-config is available
                find_package(PkgConfig QUIET)
                if(PkgConfig_FOUND)
                    set(ZITH_HAS_PKGCONFIG ON)
                    message(STATUS "   pkg-config found")
                    
                    # Search for libffi (QUIET mode, won't fail if not found)
                    pkg_check_modules(FFI QUIET libffi)
                    
                    if(FFI_FOUND)
                        if(FFI_STATIC_LIBRARIES)
                            message(STATUS "libffi found (static libraries preferred)")
                            message(STATUS "   Include dirs: ${FFI_STATIC_INCLUDE_DIRS}")
                            set(FFI_LIBRARIES ${FFI_STATIC_LIBRARIES})
                            set(FFI_INCLUDE_DIRS ${FFI_STATIC_INCLUDE_DIRS})
                            set(ZITH_HAS_FFI ON)
                        elseif(FFI_LIBRARIES)
                            message(STATUS "libffi found (dynamic libraries)")
                            message(STATUS "   Include dirs: ${FFI_INCLUDE_DIRS}")
                            set(ZITH_HAS_FFI ON)
                        else()
                            message(WARNING "libffi pkg-config found but no libraries discovered")
                            set(ZITH_HAS_FFI OFF)
                        endif()
                    else()
                        message(STATUS "libffi not found via pkg-config")
                        set(ZITH_HAS_FFI OFF)
                    endif()
                else()
                    message(STATUS "pkg-config not found; skipping libffi discovery")
                    set(ZITH_HAS_FFI OFF)
                endif()
                
                if(ZITH_HAS_FFI)
                    message(STATUS "FFI support will be compiled in")
                else()
                    message(STATUS "FFI support disabled (libffi not available)")
                endif()
            else()
                message(STATUS "FFI support disabled by user (ZITH_ENABLE_FFI=OFF)")
                set(ZITH_HAS_FFI OFF)
            endif()
        else()
            if(WIN32)
                message(STATUS "FFI not available on Windows platform")
            elseif(EMSCRIPTEN)
                message(STATUS "FFI not available in WebAssembly environment")
            endif()
            set(ZITH_HAS_FFI OFF)
        endif()

    else()
        message(STATUS "LLVM not found")
        if(ZITH_ENABLE_LLVM)
            message(WARNING "LLVM was requested but not found. Building without LLVM backend.")
            message(STATUS "   To use LLVM, install it and set -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm")
            message(STATUS "   To silence this warning, use -DZITH_ENABLE_LLVM=OFF")
        endif()
        set(LLVM_FOUND FALSE)
        set(ZITH_HAS_FFI OFF)
    endif()

else()
    message(STATUS "LLVM support disabled (ZITH_ENABLE_LLVM=OFF)")
    message(STATUS "   FFI support also disabled (depends on LLVM)")
    set(LLVM_FOUND FALSE)
    set(ZITH_HAS_FFI OFF)
endif()

option(ZITH_IS_WASM "Build target is WebAssembly" OFF)

# =============================================================================
# Compiler Flags & Warnings
# =============================================================================
option(ENABLE_WERROR "Treat warnings as errors" OFF)

if (MSVC)
    add_compile_options(/W4 /wd4100 /wd4201)
    if(ENABLE_WERROR)
        add_compile_options(/WX)
    endif()
    message(STATUS "Compiler: MSVC with warning level 4")
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(ENABLE_WERROR)
        add_compile_options(-Werror)
    endif()
    message(STATUS "Compiler: GCC/Clang with pedantic warnings enabled")

    # Linker optimization: Prefer LLD on Clang
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        find_program(LLD_EXE 
            NAMES 
                ld.lld-18 ld.lld-17 ld.lld-16 
                ld.lld 
                lld 
        )

        if(LLD_EXE)
            get_filename_component(LLD_NAME "${LLD_EXE}" NAME_WE)
            
            if (NOT CMAKE_EXE_LINKER_FLAGS MATCHES "-fuse-ld=")
                set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=${LLD_NAME}")
                set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=${LLD_NAME}")
                message(STATUS "Linker: Using LLD (${LLD_EXE})")
            endif()
        elseif(UNIX AND NOT APPLE)
            message(STATUS "Linker: LLD not found, using system default")
        endif()
    endif ()
endif ()

# =============================================================================
# Clang Extension Configuration
# =============================================================================
if(DEFINED CLANG_PLUGIN_PATH AND EXISTS "${CLANG_PLUGIN_PATH}")
    message(STATUS "Loading Clang plugin from: ${CLANG_PLUGIN_PATH}")
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:-Xclang>-load"
        "$<$<COMPILE_LANGUAGE:CXX>:-Xclang>${CLANG_PLUGIN_PATH}"
    )
endif()

# =============================================================================
# Source Files Organization
# =============================================================================
set(CORE_C_SOURCES
    ../impl/memory/arena_c_functions.c
    ../impl/memory/file.c
    ../impl/vm/encoder.cpp
    ../impl/vm/decoder.cpp
)

set(DIAGNOSTICS_SOURCES
    ../impl/diagnostics/diagnostics.cpp
    ../impl/diagnostics/diagnostic_bag.cpp
    ../impl/diagnostics/builder.cpp
    ../impl/diagnostics/span.cpp
    ../impl/diagnostics/emitter.cpp
    ../impl/diagnostics/heuristic_engine.cpp
)

set(LLVM_SOURCES
    ../impl/llvm/frontend.cpp
    ../impl/llvm/backend.cpp
)

set(PARSER_CPP_SOURCES
    ../impl/ast/ast.cpp
    ../impl/lexer/keywords.cpp
    ../impl/lexer/tokenizer.cpp
    ../impl/parser/parser.cpp
    ../impl/parser/parser_decl.cpp
    ../impl/parser/parser_expr.cpp
    ../impl/parser/parser_sema.cpp
    ../impl/parser/parser_utils.cpp
    ../impl/parser/type_checker.cpp
    ../impl/cli/project_config/project_config.cpp
)

set(CLI_SOURCES
    ../impl/cli/main.cpp
    ../impl/cli/pipeline/pipeline.cpp
    ../impl/cli/runtime_interpreted/runtime_interpreted.cpp
    ../impl/cli/cmd/build/build.cpp
    ../impl/cli/cmd/run/run.cpp
    ../impl/cli/cmd/project/project.cpp
    ../impl/cli/cmd/tool/tool.cpp
    ../impl/cli/cmd/interactive/interactive.cpp
    ../impl/cli/cmd/info/info.cpp
)

# ───────────────────────────────────────────────
# Library Target: ZithCore
# ───────────────────────────────────────────────
add_library(ZithCore STATIC ${CORE_C_SOURCES} ${DIAGNOSTICS_SOURCES})

if(LLVM_FOUND)
    target_sources(ZithCore PRIVATE ${LLVM_SOURCES})
endif()

target_include_directories(ZithCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../impl>
        $<INSTALL_INTERFACE:include>
)

# Link LLVM if available
if(LLVM_FOUND)
    target_include_directories(ZithCore SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})
    target_link_libraries(ZithCore PUBLIC ${LLVM_LIBRARIES})
endif()

target_link_libraries(ZithCore PUBLIC tomlplusplus::tomlplusplus)

# ───────────────────────────────────────────────
# Library Target: ZithParse
# ───────────────────────────────────────────────

add_library(ZithParse STATIC ${PARSER_CPP_SOURCES})

target_include_directories(ZithParse
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../impl>
)

# Add CLI11::CLI11 here because ZithParse uses it
target_link_libraries(ZithParse PUBLIC ZithCore CLI11::CLI11)

target_compile_definitions(ZithParse
    PRIVATE
        ZITH_VERSION="${ZITH_VERSION}"
)
# ... rest of properties

set_target_properties(ZithParse PROPERTIES
    VERSION 0.0.1
    SOVERSION 0
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# ───────────────────────────────────────────────
# Executable Target: zith
# ───────────────────────────────────────────────
add_executable(zith ${CLI_SOURCES})

target_include_directories(zith PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../impl
)

# Link core libraries - CLI11 is header-only, no special handling needed
target_link_libraries(zith PRIVATE ZithParse CLI11::CLI11)

# Link FFI if available (prefer static)
if(ZITH_HAS_FFI)
    target_compile_definitions(zith PRIVATE ZITH_HAS_FFI)
    target_include_directories(zith PRIVATE ${FFI_INCLUDE_DIRS})
    
    if(FFI_STATIC_LIBRARIES)
        target_link_libraries(zith PRIVATE ${FFI_STATIC_LIBRARIES})
        message(STATUS "Linking FFI (static): ${FFI_STATIC_LIBRARIES}")
    else()
        target_link_libraries(zith PRIVATE ${FFI_LIBRARIES})
        message(STATUS "Linking FFI (dynamic): ${FFI_LIBRARIES}")
    endif()
endif()

# WebAssembly target
if(ZITH_IS_WASM)
    target_compile_definitions(zith PRIVATE ZITH_WASM)
    message(STATUS "WebAssembly target enabled")
endif()

# Version and installation defines
target_compile_definitions(zith PRIVATE
    ZITH_VERSION="${ZITH_VERSION}"
    ZITH_EXECUTABLE
)

if(CMAKE_INSTALL_PREFIX)
    set(ZITH_INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/zith/lib")
    target_compile_definitions(zith PRIVATE ZITH_INSTALL_LIB_DIR="${ZITH_INSTALL_LIB_DIR}")
endif()

# ───────────────────────────────────────────────
# Executable Target: zith-lsp
# ───────────────────────────────────────────────
set(LSP_SOURCES
    ../lsp/main.cpp
    ../lsp/message_handler.cpp
    ../lsp/document_manager.cpp
    ../lsp/handlers/diagnostics.cpp
    ../lsp/handlers/semantic_tokens.cpp
    ../lsp/handlers/execute_command.cpp
)

add_executable(zith-lsp ${LSP_SOURCES})

target_include_directories(zith-lsp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../impl
)

target_link_libraries(zith-lsp PRIVATE ZithCore ZithParse nlohmann_json::nlohmann_json)

target_compile_definitions(zith-lsp PRIVATE ZITH_VERSION="${ZITH_VERSION}")

# ───────────────────────────────────────────────
# Install Targets
# ───────────────────────────────────────────────
install(TARGETS zith zith-lsp ZithParse ZithCore
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ../lib/
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/zith/lib
    FILES_MATCHING PATTERN "*.zith"
)

# =============================================================================
# Code Formatting
# =============================================================================
find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format-18 clang-format-17 clang-format-16)

if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ZITH_FORMAT_SOURCES
        "../impl/*.cpp" "../impl/*.hpp" "../impl/*.h" "../impl/*.c"
        "../include/*.hpp" "../include/*.h"
        "../tests/*.cpp"
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
    
    message(STATUS "Code formatting targets available: 'fmt' and 'fmt-check'")
else()
    message(STATUS "clang-format not found; formatting targets unavailable")
endif()

# =============================================================================
# Testing Configuration
# =============================================================================
include(CTest)

if(BUILD_TESTING)
    message(STATUS "Tests enabled; fetching Catch2...")
    
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    set(TEST_SOURCES
        ../impl/parser/parser_test.cpp
        ../tests/test_diagnostics_new.cpp
        ../tests/test_expand_sema.cpp
        ../tests/test_import.cpp
        ../tests/test_import_system.cpp
        ../tests/test_main.cpp
        ../tests/test_parser.cpp
        ../tests/test_project_config.cpp
    )

    if(TEST_SOURCES)
        add_executable(zith_tests ${TEST_SOURCES})
        
        target_include_directories(zith_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../impl
        )
        
        target_compile_definitions(zith_tests PRIVATE ZITH_VERSION="${ZITH_VERSION}")

        target_link_libraries(zith_tests PRIVATE ZithParse Catch2::Catch2WithMain)

        include(Catch)

        if(CMAKE_CROSSCOMPILING)
            message(STATUS "Cross-compiling detected; registering manual test target")
            add_test(NAME zith_tests COMMAND zith_tests)
        else()
            message(STATUS "Automatically discovering Catch2 tests")
            catch_discover_tests(zith_tests)
        endif()
    endif()

    # CLI integration test
    add_test(
        NAME zith_cli_hello_interpreted
        COMMAND $<TARGET_FILE:zith> run --interpreted ${CMAKE_SOURCE_DIR}/examples/start/hello-world.zith
    )
    message(STATUS "CLI integration test registered")
else()
    message(STATUS "Tests disabled (use -DBUILD_TESTING=ON to enable)")
endif()

# =============================================================================
# Build Summary
# =============================================================================
message("")
message(STATUS "========================================")
message(STATUS "Zith Build Configuration Summary")
message(STATUS "========================================")
message(STATUS "   Version:           ${ZITH_VERSION}")
message(STATUS "   LLVM Support:      ${LLVM_FOUND}")
message(STATUS "   FFI Support:       ${ZITH_HAS_FFI}")
message(STATUS "   WebAssembly:       ${ZITH_IS_WASM}")
if (TARGET zith)
    message(STATUS "   Executable:        zith (configured)")
endif()
if (TARGET zith_tests)
    message(STATUS "   Tests:             zith_tests (configured)")
endif()
message(STATUS "========================================")
message("")

# =============================================================================
# WebAssembly Support (Emscripten)
# =============================================================================
option(ZITH_BUILD_WASM "Build WebAssembly target" OFF)

if(ZITH_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(WARNING "ZITH_BUILD_WASM=ON but not compiling with Emscripten (emcc). Skipping WASM target.")
    else()
        message(STATUS "Configuring WebAssembly target...")

        add_executable(zith-wasm ${CLI_SOURCES})

        target_include_directories(zith-wasm PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../impl
        )

        target_link_libraries(zith-wasm PRIVATE ZithParse CLI11::CLI11)

        target_compile_definitions(zith-wasm PRIVATE
            ZITH_VERSION="${ZITH_VERSION}"
            ZITH_EXECUTABLE
            ZITH_WASM
        )

        target_link_options(zith-wasm PRIVATE
            "SHELL:-O3"
            "SHELL:-flto"
            "SHELL:--closure 1"
            "SHELL:-s WASM=1"
            "SHELL:-s FILESYSTEM=1"
            "SHELL:-s ENVIRONMENT=web,node"
            "SHELL:-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
            "SHELL:-s ALLOW_MEMORY_GROWTH=1"
        )

        set_target_properties(zith-wasm PROPERTIES
            SUFFIX ".html"
            OUTPUT_NAME "zith"
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        )

        if (MSVC)
            target_compile_options(zith-wasm PRIVATE /W4 /wd4100)
        else ()
            target_compile_options(zith-wasm PRIVATE -Wall -Wextra -Wpedantic -Wno-error)
        endif()

        install(TARGETS zith-wasm DESTINATION bin)
        message(STATUS "WebAssembly target configured")
    endif()
endif()

install(TARGETS zith DESTINATION bin)