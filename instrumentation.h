#ifndef _INSTRUMENTATION_H
#define _INSTRUMENTATION_H

static int __num_adds = 0;
static int __num_muls = 0;
static int __num_divs = 0;
static int __num_sqrts = 0;

#define NUM_ADDS(n) (__num_adds += (n))
#define NUM_MULS(n) (__num_muls += (n))
#define NUM_DIVS(n) (__num_fmas += (n))
#define NUM_SQRTS(n) (__num_sqrts += (n))
#define NUM_FLOPS (__num_adds + __num_muls + __num_divs + __num_sqrts)
#define RESET_OPS __num_adds = 0; __num_muls = 0; __num_divs = 0; __num_sqrts = 0

#endif