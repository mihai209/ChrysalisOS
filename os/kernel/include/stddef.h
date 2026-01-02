#pragma once

/* minimal stddef for freestanding kernel */

/* pointer / size types */
typedef unsigned int size_t;
typedef int ptrdiff_t;

/* NULL: in C++ use 0 (convertibil la orice pointer), in C use (void*)0 */
#ifdef __cplusplus
  #ifndef NULL
    #define NULL 0
  #endif
#else
  #ifndef NULL
    #define NULL ((void*)0)
  #endif
#endif
