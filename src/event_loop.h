#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "event_queue.h"

// 初始化事件循环
void init_event_loop(EventQueue* event_queue);

// 清理事件循环资源
void cleanup_event_loop(void);

// 运行事件循环
void run_event_loop(void);

// 停止事件循环
void stop_event_loop(void);

// 添加定时器事件
void add_timer_event(int timeout_ms, void (*callback)(void*), void* arg);

// 添加文件描述符事件
void add_fd_event(int fd, short events, void (*callback)(int, short, void*), void* arg);

#endif // EVENT_LOOP_H    