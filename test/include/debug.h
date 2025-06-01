#ifndef __DEBUG_H__
    #define __DEBUG_H__

#include <stdio.h>

// 定义宏替换printf
// #define tk_debug(format, ...) \
//     printf("[Thread 0x%lx] " format, (unsigned long)pthread_self(), ##__VA_ARGS__)

#define DEBUG_TEST 1

extern void tk_debug_internal(int control, const char *format, ...);
#define tk_debug(format, ...) tk_debug_internal(1, format, ##__VA_ARGS__)
extern void reset_debug_prefix(char *prefix);

#endif