#pragma once

#include <math.h>

#include "katana_types.h"

static inline f32 kabsf(f32 f)
{
    i32 i = ((*(int *)&f) & 0x7fffffff);
    return (*(float *)&i);
}

static inline f32 kcosf(f32 theta)
{
    return cosf(theta);
}

static inline f32 ksinf(f32 theta)
{
    return sinf(theta);
}

static inline f32 kclampf(f32 value, f32 min, f32 max)
{
    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    return value;
}

static inline u32 kclamp(u32 value, u32 min, u32 max)
{
    if (value < min) {
        value = min;
    }
    if (value > max) {
        value = max;
    }

    return value;
}

static inline f32 klerp(f32 a, f32 b, f32 t)
{
    return (1.0f - t) * a + t * b;
}

static inline f32 ksqrt(f32 a)
{
    return sqrtf(a);
}

static inline f32 kmax(f32 a, f32 b)
{
    return a > b ? a : b;
}
