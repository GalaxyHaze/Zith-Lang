#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZithArena ZithArena;

ZithArena *zith_arena_create(size_t initial_block_size);
void *zith_arena_alloc(ZithArena *arena, size_t size);
char *zith_arena_strdup(ZithArena *arena, const char *str);
char *zith_arena_str(ZithArena *arena, const char *str, size_t len);
void zith_arena_reset(ZithArena *arena);
void zith_arena_destroy(ZithArena *arena);

#ifdef __cplusplus
}
#endif
