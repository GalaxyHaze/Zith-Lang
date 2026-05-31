cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(zithc LANGUAGES C CXX)

set(BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED ZITH_VERSION OR ZITH_VERSION STREQUAL "")
    set(ZITH_VERSION "v0.0.1")
endif()
set(ZITH_VERSION "${ZITH_VERSION}" CACHE STRING "Project version" FORCE)

# --- Integração do 'mio' via FetchContent ---
include(FetchContent)

FetchContent_Declare(
    mio
    GIT_REPOSITORY https://github.com/vimpunk/mio.git
    GIT_TAG        master # Ou use um commit hash específico para maior estabilidade
)

# Transforma o mio num target disponível no CMake
FetchContent_MakeAvailable(mio)
# --------------------------------------------

# --------------------------------------------

file(GLOB_RECURSE ZITH_SOURCES
    src/*.cpp
)

add_executable(zithc ${ZITH_SOURCES})

target_include_directories(zithc PRIVATE
        src
)

# Vincula o 'mio' ao seu projeto (ele adiciona os includes automaticamente)
target_link_libraries(zithc PRIVATE mio)

# --- Testes Unitários ---

function(add_zith_test NAME SOURCE)
    add_executable(${NAME} ${SOURCE})
    target_include_directories(${NAME} PRIVATE src)
    target_link_libraries(${NAME} PRIVATE mio)
    set_target_properties(${NAME} PROPERTIES FOLDER "tests")
endfunction()

add_zith_test(zith-lexer-test  tests/unit/lexer-tokens.cpp)
add_zith_test(zith-parser-expr  tests/unit/parser-expr.cpp)
add_zith_test(zith-parser-stmt  tests/unit/parser-stmt.cpp)
add_zith_test(zith-sema-test    tests/unit/sema-basics.cpp)
add_zith_test(zith-mir-test     tests/unit/mir-lowering.cpp)
