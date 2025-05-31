#ifndef __DEBUG_H__
    #define __DEBUG_H__

// 定义宏替换printf
// #define tk_debug(format, ...) \
//     printf("[Thread 0x%lx] " format, (unsigned long)pthread_self(), ##__VA_ARGS__)

extern void tk_debug(const char *format, ...);
extern void reset_debug_prefix(char *prefix);

#endif