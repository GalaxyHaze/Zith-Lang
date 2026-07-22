add_library(zith-core STATIC
    arena.cpp
    string-interner.cpp
    source-file.cpp
    source-map.cpp
)

target_include_directories(zith-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(zith-core PUBLIC frozen::frozen)

target_compile_features(zith-core PUBLIC cxx_std_23)

if(ZITH_IS_WASM)
    target_compile_definitions(zith-core PUBLIC ZITH_IS_WASM=1)
endif()
