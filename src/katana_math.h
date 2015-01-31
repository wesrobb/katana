#pragma once

#include "katana_types.h"

static inline f32 katana_absf(f32 f)
{
        i32 i = ((*(int *)&f) & 0x7fffffff);
        return (*(float *)&i);
}
