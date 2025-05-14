#include "event_queue.h"
#include <stdlib.h>
#include <pthread.h>

// 事件节点结构
typedef struct EventNode {
    Event event;
    struct EventNode* next;
} EventNode;

// 事件队列实现
struct EventQueue {
    EventNode* head;
    EventNode* tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
};

EventQueue* create_event_queue(void) {
    EventQueue* queue = (EventQueue*)malloc(sizeof(EventQueue));
    if (!queue) return NULL;
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    
    return queue;
}

void destroy_event_queue(EventQueue* queue) {
    if (!queue) return;
    
    // 清空队列
    pthread_mutex_lock(&queue->mutex);
    EventNode* node = queue->head;
    while (node) {
        EventNode* next = node->next;
        free(node);
        node = next;
    }
    pthread_mutex_unlock(&queue->mutex);
    
    // 销毁同步原语
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    
    free(queue);
}

void enqueue_event(EventQueue* queue, const Event* event) {
    if (!queue || !event) return;
    
    pthread_mutex_lock(&queue->mutex);
    
    // 创建新节点
    EventNode* node = (EventNode*)malloc(sizeof(EventNode));
    if (!node) {
        pthread_mutex_unlock(&queue->mutex);
        return;
    }
    
    node->event = *event;
    node->next = NULL;
    
    // 添加到队列尾部
    if (queue->tail) {
        queue->tail->next = node;
        queue->tail = node;
    } else {
        queue->head = queue->tail = node;
    }
    
    queue->count++;
    
    // 通知等待的线程
    pthread_cond_signal(&queue->cond);
    
    pthread_mutex_unlock(&queue->mutex);
}

Event* dequeue_event(EventQueue* queue) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    // 队列为空，返回NULL
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    // 取出队列头部
    EventNode* node = queue->head;
    queue->head = node->next;
    
    if (!queue->head) {
        queue->tail = NULL;
    }
    
    queue->count--;
    
    // 复制事件
    Event* event = (Event*)malloc(sizeof(Event));
    if (event) {
        *event = node->event;
    }
    
    // 释放节点
    free(node);
    
    pthread_mutex_unlock(&queue->mutex);
    
    return event;
}

Event* create_event(EventType type) {
    Event* event = (Event*)malloc(sizeof(Event));
    if (event) {
        event->type = type;
        // 初始化其他字段为0
        memset(&event->data, 0, sizeof(event->data));
    }
    return event;
}

void free_event(Event* event) {
    if (event) {
        free(event);
    }
}