// impl/types/types.hpp — C++ type aliases for Zith
#pragma once

#include "zith/types.h"

#ifdef __cplusplus
namespace zith {
    using TokenType = ZithTokenType;
    using SourceLoc = ZithSourceLoc;
    using Token = ZithToken;
    using TokenStream = ZithTokenStream;
    using Str = ZithStr;
}
#endif
