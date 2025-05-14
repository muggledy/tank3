#include "game_state.h"
#include "event_queue.h"
#include <stdlib.h>
#include <pthread.h>
#include <math.h>

// 共享游戏状态
static GameState shared_game_state;
static pthread_mutex_t game_state_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_game_state(GameState* state) {
    if (!state) return;
    
    // 初始化坦克
    state->tank_count = 1; // 1个玩家坦克
    state->tanks[0].id = 0;
    state->tanks[0].position.x = 200;
    state->tanks[0].position.y = 300;
    state->tanks[0].angle = 0;
    state->tanks[0].speed = 5.0f;
    state->tanks[0].health = 100;
    state->tanks[0].score = 0;
    
    // 初始化墙体（简单示例）
    state->block_count = 5;
    for (int i = 0; i < state->block_count; i++) {
        state->blocks[i].position.x = 100 + i * 100;
        state->blocks[i].position.y = 200;
        state->blocks[i].width = 50;
        state->blocks[i].height = 50;
        state->blocks[i].type = 0; // 普通墙
    }
    
    // 初始化其他状态
    state->shell_count = 0;
    state->game_time = 0;
    state->score = 0;
    state->game_over = 0;
    
    // 复制到共享状态
    pthread_mutex_lock(&game_state_mutex);
    shared_game_state = *state;
    pthread_mutex_unlock(&game_state_mutex);
}

void update_game_state(GameState* state) {
    if (!state) return;
    
    // 更新坦克位置
    for (int i = 0; i < state->tank_count; i++) {
        // 简单AI坦克逻辑（仅演示）
        if (i > 0) {
            // 随机移动和射击
            static int move_timer = 0;
            if (++move_timer >= 60) {
                state->tanks[i].angle = (float)rand() / RAND_MAX * 360.0f;
                move_timer = 0;
                
                // 有10%的概率射击
                if (rand() % 10 == 0) {
                    Event* event = create_event(EVENT_TANK_SHOOT);
                    event->data.shoot.tank_id = i;
                    handle_event(state, event);
                    free_event(event);
                }
            }
            
            // 更新位置
            float dx = cosf(state->tanks[i].angle * M_PI / 180.0f) * state->tanks[i].speed;
            float dy = sinf(state->tanks[i].angle * M_PI / 180.0f) * state->tanks[i].speed;
            state->tanks[i].position.x += dx;
            state->tanks[i].position.y += dy;
            
            // 边界检查
            if (state->tanks[i].position.x < 0) state->tanks[i].position.x = 0;
            if (state->tanks[i].position.x > 800) state->tanks[i].position.x = 800;
            if (state->tanks[i].position.y < 0) state->tanks[i].position.y = 0;
            if (state->tanks[i].position.y > 600) state->tanks[i].position.y = 600;
            
            // 简单的碰撞检测（与墙体）
            for (int j = 0; j < state->block_count; j++) {
                if (state->tanks[i].position.x < state->blocks[j].position.x + state->blocks[j].width &&
                    state->tanks[i].position.x + 40 > state->blocks[j].position.x &&
                    state->tanks[i].position.y < state->blocks[j].position.y + state->blocks[j].height &&
                    state->tanks[i].position.y + 40 > state->blocks[j].position.y) {
                    // 碰撞发生，回退位置
                    state->tanks[i].position.x -= dx;
                    state->tanks[i].position.y -= dy;
                    break;
                }
            }
        }
    }
    
    // 更新炮弹位置
    for (int i = 0; i < state->shell_count; i++) {
        state->shells[i].position.x += state->shells[i].velocity.x;
        state->shells[i].position.y += state->shells[i].velocity.y;
        
        // 边界检查
        if (state->shells[i].position.x < 0 || state->shells[i].position.x > 800 ||
            state->shells[i].position.y < 0 || state->shells[i].position.y > 600) {
            // 移除炮弹（通过将最后一个炮弹移到当前位置）
            if (i < state->shell_count - 1) {
                state->shells[i] = state->shells[state->shell_count - 1];
            }
            state->shell_count--;
            i--; // 重新检查当前位置
            continue;
        }
        
        // 简单碰撞检测（炮弹与墙体）
        for (int j = 0; j < state->block_count; j++) {
            if (state->shells[i].position.x > state->blocks[j].position.x &&
                state->shells[i].position.x < state->blocks[j].position.x + state->blocks[j].width &&
                state->shells[i].position.y > state->blocks[j].position.y &&
                state->shells[i].position.y < state->blocks[j].position.y + state->blocks[j].height) {
                // 移除炮弹
                if (i < state->shell_count - 1) {
                    state->shells[i] = state->shells[state->shell_count - 1];
                }
                state->shell_count--;
                i--;
                break;
            }
        }
        
        // 炮弹与坦克碰撞检测
        for (int j = 0; j < state->tank_count; j++) {
            if (state->shells[i].owner_id == j) continue; // 不能击中自己
            
            float dx = state->shells[i].position.x - state->tanks[j].position.x;
            float dy = state->shells[i].position.y - state->tanks[j].position.y;
            float distance = sqrtf(dx * dx + dy * dy);
            
            if (distance < 20.0f) { // 假设坦克半径为20
                // 击中坦克，减少生命值
                state->tanks[j].health -= 25;
                
                // 移除炮弹
                if (i < state->shell_count - 1) {
                    state->shells[i] = state->shells[state->shell_count - 1];
                }
                state->shell_count--;
                i--;
                
                // 检查坦克是否被摧毁
                if (state->tanks[j].health <= 0) {
                    if (j == 0) {
                        // 玩家坦克被摧毁
                        state->game_over = 1;
                    } else {
                        // AI坦克被摧毁，增加分数
                        state->tanks[0].score += 100;
                        // 重新生成AI坦克
                        state->tanks[j].health = 100;
                        state->tanks[j].position.x = rand() % 700 + 50;
                        state->tanks[j].position.y = rand() % 500 + 50;
                    }
                }
                
                break;
            }
        }
    }
    
    // 更新游戏时间
    state->game_time++;
    
    // 复制到共享状态
    pthread_mutex_lock(&game_state_mutex);
    shared_game_state = *state;
    pthread_mutex_unlock(&game_state_mutex);
}

void handle_event(GameState* state, const Event* event) {
    if (!state || !event) return;
    
    switch (event->type) {
        case EVENT_KEY_PRESS:
            // 处理按键按下事件
            switch (event->data.key.key) {
                case KEY_UP:
                    // 向前移动
                    state->tanks[0].angle = 0;
                    break;
                case KEY_DOWN:
                    // 向后移动
                    state->tanks[0].angle = 180;
                    break;
                case KEY_LEFT:
                    // 向左移动
                    state->tanks[0].angle = 270;
                    break;
                case KEY_RIGHT:
                    // 向右移动
                    state->tanks[0].angle = 90;
                    break;
                case KEY_SPACE:
                    // 射击
                    if (state->shell_count < 32) {
                        int shell_id = state->shell_count;
                        state->shells[shell_id].id = shell_id;
                        state->shells[shell_id].owner_id = 0; // 玩家坦克发射
                        state->shells[shell_id].position = state->tanks[0].position;
                        
                        // 设置炮弹速度
                        float angle_rad = state->tanks[0].angle * M_PI / 180.0f;
                        state->shells[shell_id].velocity.x = cosf(angle_rad) * 8.0f;
                        state->shells[shell_id].velocity.y = sinf(angle_rad) * 8.0f;
                        
                        state->shell_count++;
                    }
                    break;
                case KEY_ESC:
                    // 退出游戏
                    state->game_over = 1;
                    break;
            }
            break;
            
        case EVENT_TANK_MOVE:
            // 更新坦克位置（用于网络同步或AI）
            if (event->data.move.tank_id < state->tank_count) {
                state->tanks[event->data.move.tank_id].position.x = event->data.move.x;
                state->tanks[event->data.move.tank_id].position.y = event->data.move.y;
                state->tanks[event->data.move.tank_id].angle = event->data.move.angle;
            }
            break;
            
        case EVENT_TANK_SHOOT:
            // 坦克射击（用于网络同步或AI）
            if (event->data.shoot.tank_id < state->tank_count && state->shell_count < 32) {
                int shell_id = state->shell_count;
                state->shells[shell_id].id = shell_id;
                state->shells[shell_id].owner_id = event->data.shoot.tank_id;
                state->shells[shell_id].position = state->tanks[event->data.shoot.tank_id].position;
                
                // 设置炮弹速度
                float angle_rad = state->tanks[event->data.shoot.tank_id].angle * M_PI / 180.0f;
                state->shells[shell_id].velocity.x = cosf(angle_rad) * 8.0f;
                state->shells[shell_id].velocity.y = sinf(angle_rad) * 8.0f;
                
                state->shell_count++;
            }
            break;
            
        case EVENT_QUIT:
            // 退出游戏
            state->game_over = 1;
            break;
            
        default:
            break;
    }
}

void get_game_state(GameState* state) {
    if (!state) return;
    
    pthread_mutex_lock(&game_state_mutex);
    *state = shared_game_state;
    pthread_mutex_unlock(&game_state_mutex);
}