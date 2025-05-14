#include "network.h"
#include "event_loop.h"
#include "event_queue.h"
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 服务器地址和端口
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888

// 网络状态
typedef enum {
    NETWORK_DISCONNECTED,
    NETWORK_CONNECTING,
    NETWORK_CONNECTED
} NetworkState;

// 网络上下文
typedef struct {
    struct bufferevent* bev;
    NetworkState state;
    NetworkCallback message_callback;
    void* user_data;
} NetworkContext;

// 全局网络上下文
static NetworkContext* net_ctx = NULL;

// 读取回调函数
static void read_callback(struct bufferevent* bev, void* ctx) {
    NetworkContext* nc = (NetworkContext*)ctx;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    
    if (len > 0) {
        uint8_t* data = (uint8_t*)malloc(len);
        if (data) {
            evbuffer_remove(input, data, len);
            
            // 调用消息回调
            if (nc->message_callback) {
                nc->message_callback(data, len, nc->user_data);
            }
            
            free(data);
        }
    }
}

// 写入回调函数
static void write_callback(struct bufferevent* bev, void* ctx) {
    // 可以处理写入完成的情况
}

// 事件回调函数
static void event_callback(struct bufferevent* bev, short events, void* ctx) {
    NetworkContext* nc = (NetworkContext*)ctx;
    
    if (events & BEV_EVENT_CONNECTED) {
        // 连接成功
        nc->state = NETWORK_CONNECTED;
        printf("Connected to server\n");
        
        // 发送连接成功事件到事件队列
        Event* event = create_event(EVENT_NETWORK_CONNECTED);
        enqueue_event(event_queue, event);
        free_event(event);
        notify_event_loop(); // 通知事件循环有新事件
    } else if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
        // 连接错误或断开
        printf("Connection error\n");
        
        if (nc->state == NETWORK_CONNECTED) {
            // 发送断开连接事件到事件队列
            Event* event = create_event(EVENT_NETWORK_DISCONNECTED);
            enqueue_event(event_queue, event);
            free_event(event);
            notify_event_loop(); // 通知事件循环有新事件
        }
        
        nc->state = NETWORK_DISCONNECTED;
        
        // 释放资源
        bufferevent_free(bev);
        nc->bev = NULL;
        
        // 尝试重新连接
        add_timer_event(5000, (void (*)(void*))start_network, NULL);
    }
}

// 初始化网络
void init_network(void) {
    net_ctx = (NetworkContext*)malloc(sizeof(NetworkContext));
    if (!net_ctx) {
        fprintf(stderr, "Failed to allocate network context\n");
        return;
    }
    
    memset(net_ctx, 0, sizeof(NetworkContext));
    net_ctx->state = NETWORK_DISCONNECTED;
}

// 清理网络资源
void cleanup_network(void) {
    if (!net_ctx) return;
    
    if (net_ctx->bev) {
        bufferevent_free(net_ctx->bev);
        net_ctx->bev = NULL;
    }
    
    free(net_ctx);
    net_ctx = NULL;
}

// 启动网络连接
void start_network(void) {
    if (!net_ctx || net_ctx->state != NETWORK_DISCONNECTED) return;
    
    struct event_base* base = get_event_base(); // 获取事件基
    if (!base) return;
    
    // 创建bufferevent
    net_ctx->bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!net_ctx->bev) {
        fprintf(stderr, "Failed to create bufferevent\n");
        return;
    }
    
    // 设置回调函数
    bufferevent_setcb(net_ctx->bev, read_callback, write_callback, event_callback, net_ctx);
    bufferevent_enable(net_ctx->bev, EV_READ | EV_WRITE);
    
    // 连接服务器
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(SERVER_PORT);
    evutil_inet_pton(AF_INET, SERVER_IP, &sin.sin_addr);
    
    int result = bufferevent_socket_connect(net_ctx->bev, (struct sockaddr*)&sin, sizeof(sin));
    if (result < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        bufferevent_free(net_ctx->bev);
        net_ctx->bev = NULL;
        return;
    }
    
    net_ctx->state = NETWORK_CONNECTING;
    printf("Connecting to server...\n");
}

// 停止网络连接
void stop_network(void) {
    if (!net_ctx || net_ctx->state == NETWORK_DISCONNECTED) return;
    
    if (net_ctx->bev) {
        bufferevent_free(net_ctx->bev);
        net_ctx->bev = NULL;
    }
    
    net_ctx->state = NETWORK_DISCONNECTED;
}

// 发送数据到服务器
void send_data_to_server(const void* data, size_t len) {
    if (!net_ctx || net_ctx->state != NETWORK_CONNECTED || !net_ctx->bev || !data || len == 0) {
        return;
    }
    
    bufferevent_write(net_ctx->bev, data, len);
}

// 设置网络消息回调
void set_network_message_callback(NetworkCallback callback, void* user_data) {
    if (!net_ctx) return;
    
    net_ctx->message_callback = callback;
    net_ctx->user_data = user_data;
}