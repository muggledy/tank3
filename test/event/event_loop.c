#include "event_loop.h"
#include <errno.h>
#include <unistd.h>
#include "game_state.h"
#include "debug.h"

// 全局事件基（仅控制线程使用）
struct event_base* tk_event_base = NULL;
// 用于线程通信的管道（线程共享，控制线程读端，GUI线程写端）
int tk_pipe_fds[2] = {-1, -1};
// 管道事件（控制线程检测到管道读事件，会从全局坦克事件队列中取出具体的键盘等事件进行handle处理）
struct event *tk_pipe_event = NULL;
// 定时器事件（用于周期更新子弹移动等游戏状态数据。定时器事件优先级最低）
struct event *tk_tank_update_timer_event = NULL;
#define TIMER_INTERVAL_MS (RENDER_FPS_MS*4)

#define TK_EVENT_PRIORITY_TOTAL_LEVEL 2
#define TK_EVENT_PRIORITY_HIGHEST_LEVEL 0
#define TK_EVENT_PRIORITY_LOWEST_LEVEL (TK_EVENT_PRIORITY_TOTAL_LEVEL-1)

// 全局坦克事件队列（控制线程、GUI线程共享）
EventQueue tk_event_queue;

extern void cleanup_event_loop(void);
extern void handle_event(Event* event);
extern struct event* add_timer_event(int timeout_ms, void (*callback)(void*), void* arg, int priority);
extern void update_game_state_timer_handle();

// 记录写端连接状态
static int writer_connected = 0;
// 管道读取回调函数
void pipe_read_callback(evutil_socket_t fd, short what, void *arg) {
    char buf[1024];
    int len;
    int total_bytes = 0;

    tk_debug_internal(DEBUG_TEST, "pipe_read_callback is called...\n");
    // 边缘触发模式下，必须读完所有数据
    while (1) {
        len = read(fd, buf, sizeof(buf));

        if (len > 0) {
            // 有数据可读
            if (!writer_connected) {
                tk_debug_internal(DEBUG_TEST, "Writer connected\n");
                writer_connected = 1;
            }
            total_bytes += len;
            tk_debug_internal(DEBUG_TEST, "Read chunk: %d bytes\n", len); // 一个字节就代表一个坦克事件
            // 处理数据
            // 从事件队列中取出事件并处理
            Event* event = dequeue_event(&tk_event_queue, 0);
            while (event) {
                handle_event(event);
                free_event(event);
                // if ((--len) <= 0) {
                //     break;
                // }
                event = dequeue_event(&tk_event_queue, 0);
            }
        } else if (len == 0) {
            // 写端关闭
            if (writer_connected) {
                tk_debug_internal(DEBUG_TEST, "Writer disconnected\n");
                writer_connected = 0;
            }
            break;
        } else {
            // 错误处理（连接仍存在，但是当前管道中已无更多数据可读）
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                // 缓冲区已空，退出循环
                if (total_bytes > 0) {
                    tk_debug_internal(DEBUG_TEST, "Finished reading %d bytes\n", total_bytes);
                }
                break;
            } else {
                // 其他错误
                perror("read error");
                break;
            }
        }
    }
}

// 初始化事件循环
int init_event_loop() {
    int i = 0;
    int ret = -1;

    init_event_queue(&tk_event_queue);
    // 创建管道用于线程间通信
    if (pipe(tk_pipe_fds) == -1) {
        tk_debug("Error: failed to create pipe\n");
        goto error;
    }
    // 设置管道为非阻塞
    for (i=0; i<(sizeof(tk_pipe_fds)/sizeof(int)); i++) {
        evutil_make_socket_nonblocking(tk_pipe_fds[i]);
    }

    // 创建全局事件基
    tk_event_base = event_base_new();
    if (!tk_event_base) {
        tk_debug("Error: failed to create event base\n");
        goto error;
    }
    event_base_priority_init(tk_event_base, TK_EVENT_PRIORITY_TOTAL_LEVEL); // 设置2个优先级级别
    // 创建管道读取事件
    tk_pipe_event = event_new(tk_event_base, tk_pipe_fds[0], 
                EV_READ | EV_PERSIST | EV_ET,  // 添加 EV_ET 标志启用边缘触发
                pipe_read_callback, NULL);
    if (!tk_pipe_event) {
        tk_debug("Error: failed to create pipe event\n");
        goto error;
    }
    // 设置高优先级（0是最高优先级）
    event_priority_set(tk_pipe_event, TK_EVENT_PRIORITY_HIGHEST_LEVEL);
    // 添加管道事件到事件基
    if (event_add(tk_pipe_event, NULL) == -1) {
        tk_debug("Error: failed to add pipe event\n");
        goto error;
    }
    tk_tank_update_timer_event = add_timer_event(TIMER_INTERVAL_MS, update_game_state_timer_handle, NULL, 
        TK_EVENT_PRIORITY_LOWEST_LEVEL);
    if (!tk_tank_update_timer_event) {
        goto error;
    }
    ret = 0;

    return ret;
error:
    cleanup_event_loop();
    return ret;
}

void close_read_end_of_pipe() {
    if (tk_pipe_fds[0] == -1) return;
    close(tk_pipe_fds[0]);
    tk_pipe_fds[0] = -1;
}

void close_write_end_of_pipe() {
    if (tk_pipe_fds[1] == -1) return;
    close(tk_pipe_fds[1]);
    tk_pipe_fds[1] = -1;
}

void cleanup_event_loop() {
    if (tk_pipe_event) {
        event_free(tk_pipe_event);
        tk_pipe_event = NULL;
    }
    if (tk_tank_update_timer_event) {
        event_free(tk_tank_update_timer_event);
        tk_tank_update_timer_event = NULL;
    }
    if (tk_event_base) {
        event_base_free(tk_event_base);
        tk_event_base = NULL;
    }
    close_read_end_of_pipe();
    close_write_end_of_pipe();
    cleanup_event_queue(&tk_event_queue);
}

// 运行事件循环
void run_event_loop() {
    if (!tk_event_base) return;

    // 进入事件循环
    event_base_dispatch(tk_event_base);
}

// 停止事件循环
void stop_event_loop() {
    if (!tk_event_base) return;

    // event_del(tk_pipe_event);
    // event_free(tk_pipe_event);
    // tk_pipe_event=NULL;

    // 退出事件循环
    // event_base_loopbreak(tk_event_base);
    // tk_event_base = NULL;
    struct timeval immediate = {0, 0};
    event_base_loopexit(tk_event_base, &immediate);
}

// 从其他线程通知事件循环有新事件要处理
void notify_event_loop() {
    if (tk_pipe_fds[1] == -1) return;

    // 向管道写入一个字节，触发管道事件
    char c = 'x';
    write(tk_pipe_fds[1], &c, 1);
}

KeyValue tk_key_value_for_control;

// 处理来自本地GUI线程或其他（TODO：如网络）的坦克事件
void handle_event(Event* event) {
    if (!event) return;
    if (!mytankptr || TST_FLAG(mytankptr, flags, TANK_DEAD)) {
        tk_debug("warning: your tank is dead, game is over\n");
        if (event->type == EVENT_QUIT) {
            goto quit_event_loop;
        }
        return;
    }
    switch (event->type) {
    case EVENT_KEY_PRESS:
    {
        tk_debug_internal(DEBUG_TEST, "recv key %d down\n", event->data.key);
        switch (event->data.key) {
            case KEY_W:
                SET_FLAG(&tk_key_value_for_control, mask, TK_KEY_W_ACTIVE);
                break;
            case KEY_S:
                SET_FLAG(&tk_key_value_for_control, mask, TK_KEY_S_ACTIVE);
                break;
            case KEY_A:
                SET_FLAG(&tk_key_value_for_control, mask, TK_KEY_A_ACTIVE);
                break;
            case KEY_D:
                SET_FLAG(&tk_key_value_for_control, mask, TK_KEY_D_ACTIVE);
                break;
        }
        handle_key(mytankptr, &tk_key_value_for_control);
        print_key_value(&tk_key_value_for_control);
    }
    break;
    case EVENT_KEY_RELEASE:
    {
        tk_debug_internal(DEBUG_TEST, "recv key %d up\n", event->data.key);
        switch (event->data.key) {
            case KEY_W:
                CLR_FLAG(&tk_key_value_for_control, mask, TK_KEY_W_ACTIVE);
                break;
            case KEY_S:
                CLR_FLAG(&tk_key_value_for_control, mask, TK_KEY_S_ACTIVE);
                break;
            case KEY_A:
                CLR_FLAG(&tk_key_value_for_control, mask, TK_KEY_A_ACTIVE);
                break;
            case KEY_D:
                CLR_FLAG(&tk_key_value_for_control, mask, TK_KEY_D_ACTIVE);
                break;
        }
        handle_key(mytankptr, &tk_key_value_for_control);
        print_key_value(&tk_key_value_for_control);
    }
    break;
    case EVENT_QUIT:
    {
quit_event_loop:
        tk_debug("recv quit event, stop_event_loop\n");
        stop_event_loop();
    }
    break;
    }
}

// 添加周期定时器事件
struct event* add_timer_event(int timeout_ms, void (*callback)(void*), void* arg, int priority) {
    if (!tk_event_base || !callback) return NULL;
    
    // 创建定时器事件
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    struct event* timer_event = event_new(tk_event_base, -1, EV_PERSIST, 
                                         (event_callback_fn)callback, arg);
    if (!timer_event) {
        tk_debug("Error: failed to create timer event\n");
        return NULL;
    }
    // 设置低优先级（1是较低的优先级）
    event_priority_set(timer_event, priority);
    // 添加定时器事件
    if (event_add(timer_event, &timeout) == -1) {
        tk_debug("Error: failed to add timer event\n");
        event_free(timer_event);
        return NULL;
    }
    return timer_event;
}

void update_game_state_timer_handle() {
    tk_debug_internal(DEBUG_TEST, "update_game_state_timer_handle\n");
    
}