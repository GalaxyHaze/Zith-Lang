#pragma once

#include "ast.hpp"

#ifdef __cplusplus
extern "C" {
#endif

ZithNode *zith_parse(ZithArena *arena, ZithTokenStream tokens);

#ifndef __cplusplus
ZithNode *zith_parse_with_source(ZithArena *arena, const char *source,
                                 size_t source_len, const char *filename,
                                 ZithTokenStream tokens,
                                 const char **import_roots,
                                 size_t import_root_count);
#else
ZithNode *zith_parse_with_source(ZithArena *arena, const char *source,
                                 size_t source_len, const char *filename,
                                 ZithTokenStream tokens,
                                 const char **import_roots = nullptr,
                                 size_t import_root_count = 0);
#endif

#ifdef __cplusplus
}
#endif
