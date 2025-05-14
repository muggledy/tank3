#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "event_queue.h"

// 最大坦克数量
#define MAX_TANKS 10
// 最大墙体数量
#define MAX_BLOCKS 20
// 最大炮弹数量
#define MAX_SHELLS 32

// 2D向量结构
typedef struct {
    float x;
    float y;
} Vector2;

// 坦克结构
typedef struct {
    int id;
    Vector2 position;
    float angle;  // 朝向角度（度）
    float speed;  // 移动速度
    int health;   // 生命值
    int score;    // 分数
} Tank;

// 墙体结构
typedef struct {
    Vector2 position;
    float width;
    float height;
    int type;  // 墙体类型
} Block;

// 炮弹结构
typedef struct {
    int id;
    int owner_id;  // 发射者ID
    Vector2 position;
    Vector2 velocity;
} Shell;

// 游戏状态结构
typedef struct {
    Tank tanks[MAX_TANKS];
    int tank_count;
    
    Block blocks[MAX_BLOCKS];
    int block_count;
    
    Shell shells[MAX_SHELLS];
    int shell_count;
    
    int game_time;  // 游戏时间（帧）
    int score;      // 总分数
    int game_over;  // 游戏是否结束
} GameState;

// 初始化游戏状态
void init_game_state(GameState* state);

// 更新游戏状态
void update_game_state(GameState* state);

// 处理事件
void handle_event(GameState* state, const Event* event);

// 获取游戏状态（线程安全）
void get_game_state(GameState* state);

#endif // GAME_STATE_H