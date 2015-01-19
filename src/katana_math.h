#pragma once

#include "katana_types.h"

static inline float katana_absf(float f)
{
        i32 i = ((*(int *)&f) & 0x7fffffff);
        return (*(float *)&i);
}
