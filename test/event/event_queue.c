#include "event_queue.h"
#include <string.h>
#include <stdlib.h>

Event* create_event(EventType type) {
    Event* event = (Event *)malloc(sizeof(Event));
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

void init_event_queue(EventQueue* queue) {
    if (!queue) return;
    TAILQ_INIT(&queue->event_queue);
    queue->count = 0;

    pthread_mutex_init(&queue->mutex_lock, NULL);
    pthread_cond_init(&queue->event_available_condition, NULL);
    pthread_cond_init(&queue->space_available_condition, NULL);
}

void cleanup_event_queue(EventQueue* queue) {
    Event *event = NULL;
    Event *tmp = NULL;

    if (!queue) return;
    // 清空队列
    pthread_mutex_lock(&queue->mutex_lock);
    TAILQ_FOREACH_SAFE(event, &queue->event_queue, chain, tmp) {
        TAILQ_REMOVE(&queue->event_queue, event, chain);
        free_event(event);
    }
    pthread_mutex_unlock(&queue->mutex_lock);

    // 销毁同步原语
    pthread_mutex_destroy(&queue->mutex_lock);
    pthread_cond_destroy(&queue->event_available_condition);
    pthread_cond_destroy(&queue->space_available_condition);
}

void enqueue_event(EventQueue* queue, Event* event) {
    if (!queue || !event) return;

    pthread_mutex_lock(&queue->mutex_lock);
    while (queue->count >= TK_EVENT_QUEUE_MAX_ITEM_NUM) {
        pthread_cond_wait(&queue->space_available_condition, &queue->mutex_lock);
    }

    // 将新事件插入到队尾
    TAILQ_INSERT_TAIL(&queue->event_queue, event, chain);
    queue->count++;

    // 通知等待的线程
    pthread_cond_signal(&queue->event_available_condition);
    pthread_mutex_unlock(&queue->mutex_lock);
}

Event* dequeue_event(EventQueue* queue, tk_uint8_t wait) {
    if (!queue) return NULL;

    pthread_mutex_lock(&queue->mutex_lock);
    while (queue->count <= 0) {
        if (!wait) {
            pthread_mutex_unlock(&queue->mutex_lock);
            return NULL;
        }
        pthread_cond_wait(&queue->event_available_condition, &queue->mutex_lock);
    }

    // 取出队列头部
    Event* first_event = TAILQ_FIRST(&queue->event_queue);
    TAILQ_REMOVE(&queue->event_queue, first_event, chain);
    queue->count--;

    // 通知等待的线程
    pthread_cond_signal(&queue->space_available_condition);
    pthread_mutex_unlock(&queue->mutex_lock);

    return first_event;
}