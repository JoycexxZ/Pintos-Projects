#ifndef __FIX_POINT_H
#define __FIX_POINT_H

#define FIX_POINT_SHIFT 16

typedef int fix_point;

fix_point int_to_fix(int n);

int fix_to_int_float(fix_point x);

int fix_to_int_round(fix_point x);

fix_point add(fix_point x, fix_point y);

fix_point sub(fix_point x, fix_point y);

fix_point add_int(fix_point x, int n);

fix_point sub_int(fix_point x, int n);

fix_point mul(fix_point x, fix_point y);

fix_point div(fix_point x, fix_point y);

fix_point mul_int(fix_point x, int n);

fix_point div_int(fix_point x ,int n);



#endif

