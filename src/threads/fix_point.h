// #ifndef __FIX_POINT_H
// #define __FIX_POINT_H

// #define FIX_POINT_SHIFT 16

// typedef int fix_point;

// fix_point int_to_fix(int n);

// int fix_to_int_float(fix_point x);

// int fix_to_int_round(fix_point x);

// fix_point add(fix_point x, fix_point y);

// fix_point sub(fix_point x, fix_point y);

// fix_point add_int(fix_point x, int n);

// fix_point sub_int(fix_point x, int n);

// fix_point mul(fix_point x, fix_point y);

// fix_point div(fix_point x, fix_point y);

// fix_point mul_int(fix_point x, int n);

// fix_point div_int(fix_point x ,int n);

//#endif

#ifndef __FIX_POINT_H
#define __FIX_POINT_H

/* Type for fix point numbers. */
typedef int fix_point;

/* Shifting q */
#define FIX_POINT_SHIFT 16

/* Convert int x to fixed point. */
#define int_to_fix(x) ((fix_point)(x << FIX_POINT_SHIFT))

/* Convert x to integer (rounding toward zero). */
#define fix_to_int_float(x) (x >> FIX_POINT_SHIFT)

/* Convert x to integer (rounding to nearest). */
#define fix_to_int_round(x) (x >= 0 ? ((x + (1 << (FIX_POINT_SHIFT - 1))) >> FIX_POINT_SHIFT) \
        : ((x - (1 << (FIX_POINT_SHIFT - 1))) >> FIX_POINT_SHIFT))

/* Add x and y. */
#define add(x,y) (x + y)

/* Add x and n. */
#define add_int(x,n) (x + (n << FIX_POINT_SHIFT))

/* Subtract y from x */
#define sub(x,y) (x - y)

/* Subtract n from x */
#define sub_int(x,n) (x - (n << FIX_POINT_SHIFT))

/* Multiply x by n */
#define mul_int(x,n) (x * n)

/* Divide x by n */
#define div_int(x,n) (x / n)

/* Multiply x by y */
#define mul(x,y) ((fix_point)(((int64_t) x) * y >> FIX_POINT_SHIFT))

/* Divide x by y */
#define div(x,y) ((fix_point)((((int64_t) x) << FIX_POINT_SHIFT) / y))

#endif 