#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdint.h>

// 事件类型枚举
typedef enum {
    EVENT_NONE,
    EVENT_KEY_PRESS,
    EVENT_KEY_RELEASE,
    EVENT_TANK_MOVE,
    EVENT_TANK_SHOOT,
    EVENT_QUIT,
    EVENT_NETWORK_CONNECTED,
    EVENT_NETWORK_DISCONNECTED
} EventType;

// 按键码枚举
typedef enum {
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_SPACE,
    KEY_ESC
} KeyCode;

// 网络消息类型（新增）
typedef enum {
    MSG_TYPE_TANK_MOVE = 1,
    MSG_TYPE_TANK_SHOOT = 2,
    MSG_TYPE_GAME_STATE = 3,
    MSG_TYPE_CLIENT_MOVE = 101,
    MSG_TYPE_CLIENT_SHOOT = 102
} MessageType;

// 事件结构
typedef struct Event {
    EventType type;
    union {
        struct {
            KeyCode key;
        } key;
        struct {
            int tank_id;
            float x, y;
            float angle;
        } move;
        struct {
            int tank_id;
        } shoot;
        struct {
            int status;
        } network;
    } data;
} Event;

// 事件队列结构（前向声明）
typedef struct EventQueue EventQueue;

// 创建事件队列
EventQueue* create_event_queue(void);

// 销毁事件队列
void destroy_event_queue(EventQueue* queue);

// 入队事件（线程安全）
void enqueue_event(EventQueue* queue, const Event* event);

// 出队事件（线程安全，队列为空时返回NULL）
Event* dequeue_event(EventQueue* queue);

// 创建事件
Event* create_event(EventType type);

// 释放事件
void free_event(Event* event);

#endif // EVENT_QUEUE_H