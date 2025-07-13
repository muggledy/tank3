#ifndef __EVENT_QUEUE_H__
    #define __EVENT_QUEUE_H__

#include "queue.h"
#include <pthread.h>
#include "global.h"
#include "maze.h"

// 事件类型枚举
typedef enum {
    EVENT_KEY_PRESS,   //key down
    EVENT_KEY_RELEASE, //key up
    EVENT_QUIT,
    EVENT_GAME_STOP,  // 游戏暂停（控制线程停止碰撞检测等逻辑处理）
    EVENT_GAME_START, // 游戏开始
    EVENT_PATH_SEARCH // 地图路径搜索请求（请求数据为路径终点，起点是我的坦克当前位置）
} EventType;

// 按键码枚举
typedef enum {
    KEY_LEFT,
    KEY_RIGHT,
    KEY_FORWARD,
    KEY_BACKWARD,
#define KEY_W KEY_FORWARD
#define KEY_S KEY_BACKWARD
#define KEY_A KEY_LEFT
#define KEY_D KEY_RIGHT
    KEY_SPACE,
    KEY_ESC
} KeyCode;

typedef struct {
    Grid end;
} MazePathSearchRequest;

// 事件节点结构
typedef struct _Event {
    EventType type;
    union {
        KeyCode key;
        MazePathSearchRequest path_search_request;
    } data;
    TAILQ_ENTRY(_Event) chain;
} Event;

// 事件队列实现
typedef struct {
    TAILQ_HEAD(_tk_event_queue, _Event) event_queue;
    pthread_mutex_t mutex_lock;
    pthread_cond_t event_available_condition; // 可消耗的事件，没有则阻塞掉消费者线程，切到GUI线程（事件产生者）
#define TK_EVENT_QUEUE_MAX_ITEM_NUM 100
    pthread_cond_t space_available_condition; // 可（供GUI线程或其他生产者）插入事件的空闲位置，初始（最大）空闲位置为TK_EVENT_QUEUE_MAX_ITEM_NUM
    tk_uint32_t count;
} EventQueue;

extern Event* create_event(EventType type);
extern void free_event(Event* event);
extern void init_event_queue(EventQueue* queue);
extern void cleanup_event_queue(EventQueue* queue);
// 入队事件（线程安全）
extern void enqueue_event(EventQueue* queue, Event* event);
// 出队事件（线程安全，队列为空时返回NULL）
extern Event* dequeue_event(EventQueue* queue, tk_uint8_t wait);

#endif