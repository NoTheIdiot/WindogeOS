#ifndef __FREESTANDING_C_HDRS_STDBOOL_H
#define __FREESTANDING_C_HDRS_STDBOOL_H 1

#ifndef __cplusplus

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    /* These do not need to be defined for C23+ */
#else
#  undef bool
#  define bool _Bool

#  undef true
#  define true 1
#  undef false
#  define false 0
#endif

#endif

#undef __bool_true_false_are_defined
#define __bool_true_false_are_defined 1

#endif
