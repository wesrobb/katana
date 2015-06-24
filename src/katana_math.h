#pragma once

#include <math.h>

#include "katana_types.h"

static inline f32 kabsf(f32 f)
{
        i32 i = ((*(int *)&f) & 0x7fffffff);
        return (*(float *)&i);
}

static inline f32 kcos(f32 theta)
{
    return cosf(theta);
}

static inline f32 ksin(f32 theta)
{
    return sinf(theta);
}
