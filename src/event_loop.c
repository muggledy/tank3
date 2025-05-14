#include "event_loop.h"
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

// 全局事件基
static struct event_base* event_base = NULL;
// 用于线程通信的管道
static int pipe_fds[2] = {-1, -1};
// 管道事件
static struct event* pipe_event = NULL;
// 事件队列
static EventQueue* event_queue = NULL;
// 事件循环运行标志
static int event_loop_running = 0;

// 管道读取回调函数
static void pipe_read_callback(evutil_socket_t fd, short events, void* arg) {
    char buf[1];
    // 读取管道中的数据，只是为了清除管道
    if (read(fd, buf, 1) != 1) {
        fprintf(stderr, "Error reading from pipe\n");
    }
    
    // 从事件队列中取出事件并处理
    Event* event = dequeue_event(event_queue);
    while (event) {
        // 处理事件（这里调用控制器的事件处理函数）
        handle_event(event);
        free_event(event);
        event = dequeue_event(event_queue);
    }
}

// 初始化事件循环
void init_event_loop(EventQueue* queue) {
    event_queue = queue;
    
    // 创建管道用于线程间通信
    if (pipe(pipe_fds) == -1) {
        fprintf(stderr, "Failed to create pipe\n");
        return;
    }
    
    // 设置管道为非阻塞
    evutil_make_socket_nonblocking(pipe_fds[0]);
    evutil_make_socket_nonblocking(pipe_fds[1]);
    
    // 创建事件基
    event_base = event_base_new();
    if (!event_base) {
        fprintf(stderr, "Failed to create event base\n");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return;
    }
    
    // 创建管道读取事件
    pipe_event = event_new(event_base, pipe_fds[0], EV_READ | EV_PERSIST, 
                          pipe_read_callback, NULL);
    if (!pipe_event) {
        fprintf(stderr, "Failed to create pipe event\n");
        event_base_free(event_base);
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return;
    }
    
    // 添加管道事件到事件基
    if (event_add(pipe_event, NULL) == -1) {
        fprintf(stderr, "Failed to add pipe event\n");
        event_free(pipe_event);
        event_base_free(event_base);
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return;
    }
    
    event_loop_running = 1;
}

// 清理事件循环资源
void cleanup_event_loop(void) {
    if (pipe_event) {
        event_free(pipe_event);
        pipe_event = NULL;
    }
    
    if (event_base) {
        event_base_free(event_base);
        event_base = NULL;
    }
    
    if (pipe_fds[0] != -1) {
        close(pipe_fds[0]);
        pipe_fds[0] = -1;
    }
    
    if (pipe_fds[1] != -1) {
        close(pipe_fds[1]);
        pipe_fds[1] = -1;
    }
    
    event_loop_running = 0;
}

// 运行事件循环
void run_event_loop(void) {
    if (!event_base) return;
    
    // 进入事件循环
    event_base_dispatch(event_base);
}

// 停止事件循环
void stop_event_loop(void) {
    if (!event_base || !event_loop_running) return;
    
    // 退出事件循环
    event_base_loopbreak(event_base);
}

// 添加定时器事件
void add_timer_event(int timeout_ms, void (*callback)(void*), void* arg) {
    if (!event_base || !callback) return;
    
    // 创建定时器事件
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    struct event* timer_event = event_new(event_base, -1, EV_PERSIST, 
                                         (event_callback_fn)callback, arg);
    if (!timer_event) {
        fprintf(stderr, "Failed to create timer event\n");
        return;
    }
    
    // 添加定时器事件
    if (event_add(timer_event, &timeout) == -1) {
        fprintf(stderr, "Failed to add timer event\n");
        event_free(timer_event);
        return;
    }
}

// 添加文件描述符事件
void add_fd_event(int fd, short events, void (*callback)(int, short, void*), void* arg) {
    if (!event_base || !callback) return;
    
    // 创建文件描述符事件
    struct event* fd_event = event_new(event_base, fd, events | EV_PERSIST, 
                                      (event_callback_fn)callback, arg);
    if (!fd_event) {
        fprintf(stderr, "Failed to create fd event\n");
        return;
    }
    
    // 添加文件描述符事件
    if (event_add(fd_event, NULL) == -1) {
        fprintf(stderr, "Failed to add fd event\n");
        event_free(fd_event);
        return;
    }
}

// 从其他线程通知事件循环有新事件
void notify_event_loop(void) {
    if (pipe_fds[1] == -1) return;
    
    // 向管道写入一个字节，触发管道事件
    char c = 'x';
    write(pipe_fds[1], &c, 1);
}