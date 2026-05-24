cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(zith LANGUAGES C CXX)

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
    ${BASE_DIR}/src/*.cpp
)

add_executable(zith ${ZITH_SOURCES})

target_include_directories(zith PRIVATE
    ${BASE_DIR}/src
)

# Vincula o 'mio' ao seu projeto (ele adiciona os includes automaticamente)
target_link_libraries(zith PRIVATE mio)
