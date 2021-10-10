#include "threads/fix_point.h"
#include <stdint.h>

static fix_point f = 1 << FIX_POINT_SHIFT;

fix_point 
int_to_fix(int n)
{
    return (fix_point)n << FIX_POINT_SHIFT;
}

int 
fix_to_int_float(fix_point x)
{
    return x  >> FIX_POINT_SHIFT;
}

int 
fix_to_int_round(fix_point x)
{
    // if (x >= 0)
    //     return (x + (1 << (FIX_POINT_SHIFT-1))) >> FIX_POINT_SHIFT;
    // else
    //     return (x - (1 << (FIX_POINT_SHIFT-1))) >> FIX_POINT_SHIFT;
    return x >= 0 ? ((x + (1 << (FIX_POINT_SHIFT - 1))) >> FIX_POINT_SHIFT) \
        : ((x - (1 << (FIX_POINT_SHIFT - 1))) >> FIX_POINT_SHIFT);
}

fix_point 
add(fix_point x, fix_point y)
{
    return x + y;
}

fix_point 
sub(fix_point x, fix_point y)
{
    return x - y;
}

fix_point 
add_int(fix_point x, int n)
{
    return x + (n << FIX_POINT_SHIFT);
}

fix_point 
sub_int(fix_point x, int n)
{
    return x - (n << FIX_POINT_SHIFT);
}

fix_point 
mul(fix_point x, fix_point y)
{
    return (fix_point)((int64_t)x * y >> FIX_POINT_SHIFT);
}

fix_point 
div(fix_point x, fix_point y)
{
    return (fix_point)((int64_t)x << FIX_POINT_SHIFT /y);
}

fix_point 
mul_int(fix_point x, int n)
{
    return x * n;
}

fix_point 
div_int(fix_point x ,int n)
{
    return x / n;
}
