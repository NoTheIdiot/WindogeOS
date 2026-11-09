#ifndef STDDEF_H
#define STDDEF_H 1

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;

#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef typeof(nullptr) nullptr_t;
#endif

#endif

#ifdef __cplusplus
typedef decltype(nullptr) nullptr_t;
#endif

#undef NULL
#ifndef __cplusplus
#  define NULL ((void *)0)
#else
#  define NULL __null
#endif

#undef offsetof
#define offsetof(s, m) __builtin_offsetof(s, m)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#  undef unreachable
#  define unreachable() __builtin_unreachable()

#  define __STDC_VERSION_STDDEF_H__ 202311L
#endif

#endif
