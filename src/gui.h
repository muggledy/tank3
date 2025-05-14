#ifndef GUI_H
#define GUI_H

#include "game_state.h"

// 初始化GUI
void init_gui(void);

// 清理GUI资源
void cleanup_gui(void);

// 处理GUI事件，将事件放入队列
void handle_gui_events(EventQueue* event_queue);

// 渲染游戏状态
void render_game_state(const GameState* state);

// 检查GUI是否请求退出
int gui_should_quit(void);

#endif // GUI_H