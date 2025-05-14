#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "event_queue.h"
#include "game_state.h"

// 初始化控制器
void init_controller(EventQueue* queue);

// 清理控制器资源
void cleanup_controller(void);

// 处理事件
void handle_event(const Event* event);

// 更新游戏状态
void update_game_state(void);

// 获取游戏状态
void get_game_state(GameState* state);

// 设置游戏模式（1为在线，0为单机）
void set_game_mode(int online);

#endif // CONTROLLER_H