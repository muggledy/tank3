#ifndef __TANK_H__
    #define __TANK_H__

#include <SDL2/SDL.h>
#include "global.h"

// 颜色定义
// 定义颜色枚举
typedef enum {
    TK_WHITE,
    TK_BLACK,
    TK_RED,
    TK_GREEN,
    TK_BLUE,
    TK_YELLOW
} TKColorID;

#define ID2COLOR(colorid)    tk_colors[colorid]
#define ID2COLORPTR(colorid) &(tk_colors[colorid])

#define COLOR2PARAM(color) \
    (color).r,(color).g,(color).b,(color).a
#define COLORPTR2PARAM(colorptr) \
    (colorptr)->r,(colorptr)->g,(colorptr)->b,\
    (colorptr)->a
#define COLORPTR2PARAM2(colorptr,alpha) \
    (colorptr)->r,(colorptr)->g,(colorptr)->b,\
    (int)(((colorptr)->a)*(alpha))

#define SET_COLOR(color, r, g, b, a) do { \
    (color).r = (r); \
    (color).g = (g); \
    (color).b = (b); \
    (color).a = (a); \
} while(0)

// 2D向量结构
typedef struct __attribute__((packed)) {
    tk_float32_t x;
    tk_float32_t y;
} Vector2;

typedef Vector2 Point;

typedef struct __attribute__((packed)) {
	Point lefttop;
	Point righttop;
	Point rightbottom;
	Point leftbottom;
} Rectangle;

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
#define MAX_PARTICLES 35     // 最大粒子数
#define PARTICLE_MAX_LIFE 50 // 粒子最大存活帧
    ExplodeParticle particles[MAX_PARTICLES]; //爆炸粒子集合
    int active_count;        // 当前激活粒子数
} ExplodeEffect;

// 坦克结构
typedef struct {
    tk_uint32_t id;
    Point position;     //坦克中心点
#define TANK_LENGTH 29
#define TANK_WIDTH  23
    tk_float32_t angle_deg; // 朝向角度
    tk_float32_t speed; // 移动速度
#define TANK_INIT_SPEED 2
    tk_uint16_t health; // 生命值
    tk_uint16_t max_health;
    tk_uint16_t score;  // 分数
    ExplodeEffect explode_effect; //爆炸效果
    SDL_Color *basic_color; //基本颜色
} Tank;

#define POS(point) point.x,point.y

#endif