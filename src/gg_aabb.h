#pragma once

#include "gg_platform.h"

typedef struct {
    i32 x_min, y_min, x_max, y_max;
} aabb2i_t;

#define AABB2I(x_min, y_min, x_max, y_max) (aabb2i_t){(i32)x_min, (i32)y_min, (i32)x_max, (i32)y_max}

static inline aabb2i_t aab2i_intersect(aabb2i_t a, aabb2i_t b)
{
    aabb2i_t result;
    result.x_min = gg_max(a.x_min, b.x_min);
    result.y_min = gg_max(a.y_min, b.y_min);
    result.x_max = gg_min(a.x_max, b.x_max);
    result.y_max = gg_min(a.y_max, b.y_max);
    return result;
}

static inline aabb2i_t aabb2i_union(aabb2i_t a, aabb2i_t b)
{
    aabb2i_t result;
    result.x_min = gg_min(a.x_min, b.x_min);
    result.y_min = gg_min(a.y_min, b.y_min);
    result.x_max = gg_max(a.x_max, b.x_max);
    result.y_max = gg_max(a.y_max, b.y_max);
    return result;
}

static inline b8 aabb2i_has_area(aabb2i_t a)
{
    return (a.x_min < a.x_max) && (a.y_min < a.y_max);
}

static inline i32 aabb2i_clamped_area(aabb2i_t a)
{
    i32 width = a.x_max - a.x_min;
    i32 height =  a.y_max - a.y_min;
    i32 result = 0;
    if (width > 0 && height > 0) {
        result = width * height;
    }
    return result;
}
