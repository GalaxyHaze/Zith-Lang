#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "memory.hpp"

#ifdef __cplusplus
extern "C" {
#endif

bool zith_file_exists(const char *path);
bool zith_file_is_regular(const char *path);
size_t zith_file_size(const char *path);
bool zith_file_has_extension(const char *path, const char *ext);
char *zith_load_file_to_arena(ZithArena *arena, const char *path, size_t *out_size);
int zith_run(int argc, const char *const argv[]);

#ifdef __cplusplus
}
#endif
