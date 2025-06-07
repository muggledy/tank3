#ifndef __EVENT_LOOP_H__
    #define __EVENT_LOOP_H__

#include <event2/event.h>
#include <event2/util.h>  // 用于 evutil_make_socket_nonblocking
#include "event_queue.h"

// #define ENABLE_EVENT_PRIORITY // 启用优先级可能会导致处于最低优先级的定时器事件迟迟得不到响应，因此不建议启用

extern EventQueue tk_event_queue;

extern int init_event_loop();
extern void cleanup_event_loop();
extern void run_event_loop();
// extern void stop_event_loop();
extern void notify_event_loop();
extern void close_write_end_of_pipe();

#endif