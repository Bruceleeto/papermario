#include "ultra64.h"

#ifndef LINUX
f32 sqrtf(f32 f) {
    return __builtin_sqrtf(f);
}
#endif
