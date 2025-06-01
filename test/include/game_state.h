#ifndef __GAME_STATE_H__
    #define __GAME_STATE_H__

#include "global.h"
#include "idpool.h"
#include "queue.h"
#include "debug.h"

// 2D向量结构
typedef struct __attribute__((packed)) {
    tk_float32_t x;
    tk_float32_t y;
} Vector2;

typedef Vector2 Point;

#define POS(point) point.x,point.y

typedef struct __attribute__((packed)) {
	Point lefttop;
	Point righttop;
	Point rightbottom;
	Point leftbottom;
} Rectangle;

typedef struct {
#define TK_KEY_W_ACTIVE 0x00000001
#define TK_KEY_A_ACTIVE 0x00000002
#define TK_KEY_S_ACTIVE 0x00000004
#define TK_KEY_D_ACTIVE 0x00000008
    tk_uint32_t mask;
} KeyValue;

// 爆炸粒子
typedef struct {
    Point position;     // 粒子位置
    Point velocity;     // 速度向量（x,y方向速度）
    tk_float32_t angle; // 旋转角度
    tk_float32_t scale; // 缩放比例（0.1~1.0）
    tk_uint8_t alpha;      // 透明度（0~255）
    tk_uint8_t life;       // 剩余生命周期（0~PARTICLE_MAX_LIFE）
    tk_uint8_t shape_type; // 形状类型（0:三角形，1:四边形，2:五边形）
} ExplodeParticle;

// 爆炸效果管理器
typedef struct {
#define MAX_PARTICLES 35   // 最大粒子数
#define PARTICLE_MAX_LIFE 50 // 粒子最大存活帧
    ExplodeParticle particles[MAX_PARTICLES]; //爆炸粒子集合
    int active_count;      // 当前激活粒子数
} ExplodeEffect;

// 炮弹结构
typedef struct _Shell {
    tk_uint32_t id;
    tk_uint32_t owner_id;  // 发射者ID
    Vector2 position;
    Vector2 velocity;
    TAILQ_ENTRY(_Shell) chain;
} Shell;

// 坦克结构
typedef struct _Tank {
    tk_uint32_t id;
#define TANK_NAME_MAXLEN 32
    tk_uint8_t name[TANK_NAME_MAXLEN];
    Point position;     //坦克中心点
#define TANK_LENGTH 29
#define TANK_WIDTH  23
    tk_float32_t angle_deg; // 朝向角度
    tk_float32_t speed; // 移动速度
#define TANK_INIT_SPEED 2
    tk_uint16_t health; // 生命值
    tk_uint16_t max_health;
    tk_uint16_t score;  // 分数
#define TANK_ALIVE 0x00000001
#define TANK_DYING 0x00000002
#define TANK_DEAD  0x00000004
    tk_uint32_t flags;
    void *basic_color; //基本颜色
    ExplodeEffect explode_effect; //爆炸效果
#define TANK_ROLE_SELF  0
#define TANK_ROLE_ENEMY 1
    tk_uint8_t role;
#define DEFAULT_TANK_SHELLS_MAX_NUM 3
    tk_uint8_t max_shell_num;
    TAILQ_HEAD(_tank_shells_list, _Shell) shell_list;
    TAILQ_ENTRY(_Tank) chain;
} Tank;

// 游戏状态结构
typedef struct {
#define DEFAULT_TANK_MAX_NUM 8
    TAILQ_HEAD(_tk_tanks_list, _Tank) tank_list;
    Tank *my_tank;

    tk_uint32_t game_time; // 游戏时间（帧）
    tk_uint8_t game_over;  // 游戏是否结束
} GameState;

extern GameState tk_shared_game_state;

#define mytankptr (tk_shared_game_state.my_tank)

static void print_key_value(KeyValue *v) {
    if (!v || ((v->mask) == 0)) {
        return;
    }
    tk_debug("current key mask: ");
    if (TST_FLAG(v, mask, TK_KEY_W_ACTIVE)) {
        printf("W,");
    }
    if (TST_FLAG(v, mask, TK_KEY_S_ACTIVE)) {
        printf("S,");
    }
    if (TST_FLAG(v, mask, TK_KEY_A_ACTIVE)) {
        printf("A,");
    }
    if (TST_FLAG(v, mask, TK_KEY_D_ACTIVE)) {
        printf("D,");
    }
    printf("\n");
}

extern int init_idpool();
extern void cleanup_idpool();

extern void init_game_state();
extern void cleanup_game_state();
extern Tank* create_tank(tk_uint8_t *name, Point pos, tk_float32_t angle_deg, tk_uint8_t role);
extern void delete_tank(Tank *tank);

extern void handle_key(Tank *tank, KeyValue *key_value);

#endif