// include/zith/keyword.h — Keyword lookup (C ABI)
#pragma once

#include <stddef.h>
#include "zith/token.h"

#ifdef __cplusplus
extern "C" {
#endif

ZithTokenType zith_lookup_keyword(const char *str, size_t len);

#ifdef __cplusplus
}
#endif
