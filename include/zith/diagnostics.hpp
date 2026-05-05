#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ZITH_OK = 0,
    ZITH_ERR_IO,
    ZITH_ERR_PARSE,
    ZITH_ERR_LEX,
    ZITH_ERR_MEMORY,
    ZITH_ERR_INVALID_INPUT
} ZithError;

#ifdef __cplusplus
}
#endif
