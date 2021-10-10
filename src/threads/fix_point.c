#include "threads/fix_point.h"
#include <stdint.h>

static fix_point f = 1 << FIX_POINT_SHIFT;

fix_point 
int_to_fix(int n)
{
    return (fix_point)n * f;
}

int 
fix_to_int_float(fix_point x)
{
    return x / f;
}

int 
fix_to_int_round(fix_point x)
{
    if (x >= 0)
        return (x + f/2)/f;
    else
        return (x - f/2)/f; 
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
    return x + n*f;
}

fix_point 
sub_int(fix_point x, int n)
{
    return x - n*f;
}

fix_point 
mul(fix_point x, fix_point y)
{
    return (fix_point)((int64_t)x * y)/f;
}

fix_point 
div(fix_point x, fix_point y)
{
    return (fix_point)((int64_t)x * f)/y;
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
