#include "controller.h"
#include "event_loop.h"
#include "network.h"
#include <stdlib.h>

static EventQueue* event_queue = NULL;
static GameState game_state;
static int is_online = 0;

// 网络消息回调
static void network_message_callback(uint8_t* data, size_t len, void* user_data) {
    // 解析网络消息并转换为游戏事件
    // 这里只是一个示例，实际实现需要根据具体协议解析消息
    
    // 假设前4个字节是消息类型
    if (len < 4) return;
    
    uint32_t msg_type = *((uint32_t*)data);
    
    switch (msg_type) {
        case MSG_TYPE_TANK_MOVE:
            // 解析坦克移动消息
            if (len >= 20) {
                int tank_id = data[4];
                float x = *((float*)&data[8]);
                float y = *((float*)&data[12]);
                float angle = *((float*)&data[16]);
                
                // 创建坦克移动事件
                Event* event = create_event(EVENT_TANK_MOVE);
                event->data.move.tank_id = tank_id;
                event->data.move.x = x;
                event->data.move.y = y;
                event->data.move.angle = angle;
                
                enqueue_event(event_queue, event);
                free_event(event);
                notify_event_loop(); // 通知事件循环有新事件
            }
            break;
            
        case MSG_TYPE_TANK_SHOOT:
            // 解析坦克射击消息
            if (len >= 8) {
                int tank_id = data[4];
                
                // 创建坦克射击事件
                Event* event = create_event(EVENT_TANK_SHOOT);
                event->data.shoot.tank_id = tank_id;
                
                enqueue_event(event_queue, event);
                free_event(event);
                notify_event_loop(); // 通知事件循环有新事件
            }
            break;
            
        case MSG_TYPE_GAME_STATE:
            // 解析游戏状态消息
            // 这里可以直接更新游戏状态，而不需要通过事件
            if (len >= sizeof(GameState)) {
                memcpy(&game_state, data + 4, sizeof(GameState));
            }
            break;
            
        default:
            break;
    }
}

void init_controller(EventQueue* queue) {
    event_queue = queue;
    init_game_state(&game_state);
    
    // 初始化网络
    init_network();
    set_network_message_callback(network_message_callback, NULL);
    
    // 添加定时器事件，定期更新游戏状态
    add_timer_event(16, (void (*)(void*))update_game_state, NULL);
}

void cleanup_controller(void) {
    // 停止网络
    stop_network();
    cleanup_network();
    
    // 清理资源
}

void handle_event(const Event* event) {
    if (!event) return;
    
    // 在控制线程中处理事件
    ::handle_event(&game_state, event);
    
    // 如果是在线模式，将某些事件发送到服务器
    if (is_online) {
        switch (event->type) {
            case EVENT_TANK_MOVE:
            case EVENT_TANK_SHOOT:
                // 发送事件到服务器
                send_event_to_server(event);
                break;
                
            default:
                break;
        }
    }
}

// 发送事件到服务器
void send_event_to_server(const Event* event) {
    if (!event) return;
    
    // 这里需要根据具体协议将事件转换为网络消息
    // 简化示例，实际实现需要更复杂的协议处理
    
    uint8_t buffer[1024];
    size_t len = 0;
    
    // 设置消息头
    uint32_t msg_type = 0;
    
    switch (event->type) {
        case EVENT_TANK_MOVE:
            msg_type = MSG_TYPE_CLIENT_MOVE;
            len = 24; // 4字节类型 + 20字节数据
            *((uint32_t*)buffer) = msg_type;
            buffer[4] = event->data.move.tank_id;
            *((float*)&buffer[8]) = event->data.move.x;
            *((float*)&buffer[12]) = event->data.move.y;
            *((float*)&buffer[16]) = event->data.move.angle;
            break;
            
        case EVENT_TANK_SHOOT:
            msg_type = MSG_TYPE_CLIENT_SHOOT;
            len = 8; // 4字节类型 + 4字节数据
            *((uint32_t*)buffer) = msg_type;
            buffer[4] = event->data.shoot.tank_id;
            break;
            
        default:
            return;
    }
    
    // 发送数据到服务器
    send_data_to_server(buffer, len);
}

void update_game_state(void) {
    if (!is_online) {
        // 单机模式下更新游戏状态
        ::update_game_state(&game_state);
    } else {
        // 在线模式下，游戏状态由服务器更新
        // 这里可以处理一些本地预测或插值
    }
}

void get_game_state(GameState* state) {
    if (!state) return;
    
    // 获取游戏状态（线程安全）
    ::get_game_state(state);
}

// 设置游戏模式（单机/在线）
void set_game_mode(int online) {
    is_online = online;
    
    if (is_online) {
        // 启动网络连接
        start_network();
    } else {
        // 停止网络连接
        stop_network();
    }
}