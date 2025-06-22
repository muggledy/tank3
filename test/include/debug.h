#ifndef __DEBUG_H__
    #define __DEBUG_H__

#include <stdio.h>

// 定义宏替换printf
// #define tk_debug(format, ...) \
//     printf("[Thread 0x%lx] " format, (unsigned long)pthread_self(), ##__VA_ARGS__)

#define DEBUG_CONTROL_THREAD_DETAIL 0
#define DEBUG_EVENT_LOOP      0
#define DEBUG_SIGHT_LINE      0
#define DEBUG_TANK_COLLISION  0
#define DEBUG_SHELL_COLLISION 0
#define DEBUG_GUI_THREAD_DETAIL 0
#define DEBUG_ENEMY_MUGGLE_TANK 0

extern void tk_debug_internal(int control, const char *format, ...);
#define tk_debug(format, ...) tk_debug_internal(1, format, ##__VA_ARGS__)
extern void reset_debug_prefix(char *prefix);

#endif