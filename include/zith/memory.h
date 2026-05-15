// include/zith/memory.h — Memory arena management and file utilities (C ABI)
#pragma once
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZithArena ZithArena;

ZithArena *zith_arena_create(size_t initial_block_size);
void       zith_arena_destroy(ZithArena *arena);
void      *zith_arena_alloc(ZithArena *arena, size_t size);
char      *zith_arena_strdup(ZithArena *arena, const char *str);
char      *zith_arena_str(ZithArena *arena, const char *str, size_t len);
void       zith_arena_reset(ZithArena *arena);

// File utilities
bool       zith_file_exists(const char *path);
bool       zith_file_is_regular(const char *path);
size_t     zith_file_size(const char *path);
bool       zith_is_source_file(const char *path);
char      *zith_load_file_to_arena(ZithArena *arena, const char *path, size_t *out_size);

#ifdef __cplusplus
}
#endif
