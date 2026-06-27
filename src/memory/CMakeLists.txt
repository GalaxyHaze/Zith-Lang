add_library(zith-core STATIC
    arena.cpp
    string-interner.cpp
)

target_include_directories(zith-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(zith-core PUBLIC cxx_std_20)
