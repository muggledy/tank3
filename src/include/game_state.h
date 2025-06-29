#ifndef __GAME_STATE_H__
    #define __GAME_STATE_H__

#include "global.h"
#include "idpool.h"
#include "queue.h"
#include "debug.h"
#include "maze.h"
#include <pthread.h>

// 2D向量结构
typedef struct __attribute__((packed)) {
    tk_float32_t x;
    tk_float32_t y;
} Vector2;

typedef Vector2 Point;

#define POS(point) point.x,point.y
#define POSPTR(point) point->x,point->y

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
    tk_uint32_t id; // 对象id，游戏内可创建的对象资源是有限的，从资源分配角度，alloc id失败意味着游戏资源耗尽
    // tk_uint32_t owner_id;  // 发射者ID
    void *tank_owner; // 发射者
#define SHELL_RADIUS_LENGTH 3 // 炮弹半径
    Point position;
    tk_float32_t angle_deg; // 运动方向（同Tank->angle_deg）
    tk_float32_t speed;     // 移动速度
#define SHELL_INIT_SPEED 9  // <=10
    tk_uint8_t ttl; // 碰撞墙壁的次数，达到阈值(SHELL_COLLISION_MAX_NUM)则湮灭
#define SHELL_COLLISION_MAX_NUM 6 // TTL
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
#define TANK_INIT_SPEED 4
    tk_uint16_t health; // 生命值（要摧毁一辆坦克只需要将health减小到0）
    tk_uint16_t max_health;
    tk_uint16_t score;  // 分数
#define TANK_ALIVE 0x00000001
#define TANK_DYING 0x00000002
#define TANK_DEAD  0x00000004
#define TANK_FORBID_SHOOT 0x00000008
#define TANK_HAS_DECIDE_NEW_DIR_FOR_MUGGLE_ENEMY 0x00000100
#define TANK_IS_HIT_BY_ENEMY 0x00000200 // 如果坦克被击中则置上此标记用于播放坦克被击中的音效
    tk_uint32_t flags;
#define COLLISION_FRONT 0x01
#define COLLISION_BACK  0x02
#define COLLISION_LEFT  0x04
#define COLLISION_RIGHT 0x08
#define COLLISION_WITH_TANK 0x80
    tk_uint8_t collision_flag; //碰撞方位标记（仅低四位用于碰撞标记，高四位用于其他用途。当前高四位的最高位用于表示坦克与其他坦克发生碰撞）
    void *basic_color; //基本颜色
    ExplodeEffect explode_effect; //爆炸效果
#define TANK_ROLE_SELF  0
#define TANK_ROLE_ENEMY_MUGGLE 1  // 傻瓜敌人
    tk_uint8_t role;
#define DEFAULT_TANK_SHELLS_MAX_NUM 4
    tk_uint8_t max_shell_num;
    Rectangle outline; // 坦克轮廓边界（简化为矩形），用于碰撞检测，某一帧中，其可能已经侵入墙体
    Rectangle practical_outline; // 实际的轮廓边界，未发生碰撞的轮廓
    pthread_spinlock_t spinlock; // 理论上控制线程修改tank对象内容与GUI线程访问读取tank对象内容需要上锁保证正确，为了减小性能影响，此处我们暂用于保护对tank->shell_list的安全访问
    KeyValue key_value_for_control;
    /*for muggle enemy*/
#define STEPS_TO_ESCAPE_NUM 6
#define MOVE_FRONT 0x01
#define MOVE_BACK  0x02
#define MOVE_LEFT  0x04
#define MOVE_RIGHT 0x08
    tk_uint32_t *steps_to_escape; // 傻瓜敌人遇阻自行脱困的步骤，低四位代表方向，高四位代表动作帧数
    Grid current_grid;
    tk_uint32_t (*map_vis)[HORIZON_GRID_NUMBER]; // 标记对地图上网格的访问状态（权重矩阵，每访问一个网格，则访问权重/访问量加1）
    /*end(for muggle enemy)*/
    TAILQ_HEAD(_tank_shells_list, _Shell) shell_list;
    TAILQ_ENTRY(_Tank) chain;
} Tank;

extern Point tk_maze_offset;

// 游戏状态结构
typedef struct {
#define DEFAULT_TANK_MAX_NUM 8
    TAILQ_HEAD(_tk_tanks_list, _Tank) tank_list;
    Tank *my_tank;
    Maze maze; // 迷宫地图
    Block* blocks;          // 地图墙壁集合
    tk_uint16_t blocks_num; // 地图墙壁数量
    tk_uint32_t game_time;  // 游戏时间（帧）
    // tk_uint8_t game_over;  // 游戏是否结束
    pthread_spinlock_t spinlock; // 参考tank->spinlock，此锁则是用于保护对tk_shared_game_state.tank_list的安全访问
} GameState;

extern GameState tk_shared_game_state;

#define mytankptr (tk_shared_game_state.my_tank)
#define RENDER_FPS_MS 50

typedef struct {
    Point start_point; // pos起点
    Grid current_grid; // 当前pos所处网格
    tk_float32_t angle_deg; // 射线方向角度（正北:=0）
    /*above is input param, below is output*/
    Point intersection_dot; // 射线与当前网格边框的交点
    Grid next_grid; // 射线射入的下一个网格（如果交点所在边框未被打通，则下一个网格还是当前网格，此时出现墙壁/镜面反射，reflect_angle_deg才有值/有意义）
    double k; // 射线斜率
    tk_float32_t reflect_angle_deg; // 镜面反射后的方向角度
    tk_uint8_t terminate_flag; // 是否终止反射探测流程
} Ray_Intersection_Dot_Info;

static void print_key_value(KeyValue *v) {
    if (!DEBUG_CONTROL_THREAD_DETAIL) {
        return;
    }
    if (!v /*|| ((v->mask) == 0)*/) {
        return;
    }
    tk_debug("current key mask: ");
    if ((v->mask) == 0) {
        printf("null\n");
        return;
    }
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
extern void delete_tank(Tank *tank, int dereference);
extern Point get_random_grid_pos();
extern Point get_random_grid_pos_for_tank();

extern void handle_key(Tank *tank, KeyValue *key_value);

extern Point get_line_center(const Point *p1, const Point *p2);
extern Point rotate_point(const Point *point, tk_float32_t angle, const Point *pivot);
extern void get_ray_intersection_dot_with_grid(Ray_Intersection_Dot_Info *info);
extern Grid get_grid_by_tank_position(Point *pos);
extern Shell* create_shell_for_tank(Tank *tank);
extern void delete_shell(Shell *shell, int dereference);
extern void update_all_shell_movement_position();
extern void update_muggle_enemy_position();

extern void lock(pthread_spinlock_t *spinlock);
extern void unlock(pthread_spinlock_t *spinlock);

#endif