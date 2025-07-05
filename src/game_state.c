#include <stdlib.h>
#include <stdio.h>
#include "game_state.h"
#include <bsd/string.h>
#include <math.h>
#include "tools.h"
#include <stdbool.h>

/*山与海辞别岁晚，石与月共祝春欢*/

IDPool* tk_idpool = NULL;
GameState tk_shared_game_state;

Point tk_maze_offset = {20,20}; // 默认生成的地图左上角为(0,0)，导致地图位于窗口最左上角不太美观，整体将地图往右下移动一段偏移距离

extern Grid get_grid_by_tank_position(Point *pos);

void init_spinlock(pthread_spinlock_t *spinlock) {
    pthread_spin_init(spinlock, PTHREAD_PROCESS_PRIVATE);
}

void lock(pthread_spinlock_t *spinlock) {
    pthread_spin_lock(spinlock);
}

void unlock(pthread_spinlock_t *spinlock) {
    pthread_spin_unlock(spinlock);
}

void destroy_spinlock(pthread_spinlock_t *spinlock) {
    pthread_spin_destroy(spinlock);
}

int init_idpool() {
    tk_idpool = id_pool_create(ID_POOL_SIZE);
    if (!tk_idpool) {
        return -1;
    }
    return 0;
}

void cleanup_idpool() {
     if (tk_idpool) {
        id_pool_destroy(tk_idpool);
        tk_idpool = NULL;
    }
}

// 随机获取一个网格的中心位置
Point get_random_grid_pos() {
    return (Point){random_range(0, HORIZON_GRID_NUMBER-1) * GRID_SIZE + tk_maze_offset.x + (GRID_SIZE/2), 
        random_range(0, VERTICAL_GRID_NUMBER-1) * GRID_SIZE + tk_maze_offset.y + (GRID_SIZE/2)};
}

// 随机获取一个网格的中心位置（且尽力确保该位置没有被其他坦克所占用）
Point get_random_grid_pos_for_tank() {
    int i = 0;
    Point p;
    Tank *tank = NULL;
    Grid grid1, grid2;
    int generate_success = 1;

    for (i=0; i<300; i++) { // try generate 300(MAX) times
        p = get_random_grid_pos();
        grid1 = get_grid_by_tank_position(&p);
        generate_success = 1;

        TAILQ_FOREACH(tank, &tk_shared_game_state.tank_list, chain) {
            grid2 = get_grid_by_tank_position(&tank->position);
            if (grid_id(&grid1) == grid_id(&grid2)) {
                generate_success = 0;
                break;
            }
        }
        if (generate_success == 1) {
            return p;
        }
    }

    //return (Point){0, 0}; // generate fail
    return p;
}

extern void delete_shell(Shell *shell, int dereference);
extern void calculate_tank_outline(const Point *center, tk_float32_t width, tk_float32_t height, tk_float32_t angle_deg, Rectangle *rect);

tk_float32_t calc_corrected_angle_deg(tk_float32_t angle_deg) { // see handle_key()
    tk_float32_t corrected_angle_deg = angle_deg;
    if (corrected_angle_deg < 0) {
        corrected_angle_deg = 0;
    }
    if (corrected_angle_deg > 360) {
        corrected_angle_deg = 360;
    }
    corrected_angle_deg += 270;
    if (corrected_angle_deg >= 360) {
        corrected_angle_deg -= 360;
    }
    return corrected_angle_deg;
}

Tank* create_tank(tk_uint8_t *name, Point pos, tk_float32_t angle_deg, tk_uint8_t role) {
    Tank *tank = NULL;
    Tank *t = NULL;
    tk_uint8_t tank_num = 0;
    int map_vis_bytes = 0;

    TAILQ_FOREACH(t, &tk_shared_game_state.tank_list, chain) {
        tank_num++;
    }
    if (tank_num >= DEFAULT_TANK_MAX_NUM) {
        tk_debug("Error: create tank(%s) failed for current tank num %u already >= DEFAULT_TANK_MAX_NUM(%u)\n", 
            name, tank_num, DEFAULT_TANK_MAX_NUM);
        return NULL;
    }
    if (tk_shared_game_state.my_tank && (TANK_ROLE_SELF == role)) {
        tk_debug("Error: create my tank(%s) failed for it(%s) already exists\n", name, tk_shared_game_state.my_tank->name);
        return NULL;
    }

    tank = malloc(sizeof(Tank));
    if (!tank) {
        goto error;
    }
    memset(tank, 0, sizeof(Tank));

    strlcpy(tank->name, name, sizeof(tank->name));
    tank->id = id_pool_allocate(tk_idpool);
    if (!tank->id) {
        tk_debug("Error: %s id_pool_allocate failed\n", __func__);
        goto error;
    }
    tank->role = role;
    if (TANK_ROLE_SELF == tank->role) {
        tank->steps_to_escape = NULL;
        tank->map_vis = NULL;
    } else if (TANK_ROLE_ENEMY_MUGGLE == tank->role) {
        tank->steps_to_escape = malloc(sizeof(tk_uint32_t) * STEPS_TO_ESCAPE_NUM);
        if (!tank->steps_to_escape) {
            goto error;
        }
        memset(tank->steps_to_escape, 0, sizeof(tk_uint32_t) * STEPS_TO_ESCAPE_NUM);
        map_vis_bytes = VERTICAL_GRID_NUMBER * sizeof(*tank->map_vis);
        tank->map_vis = malloc(map_vis_bytes);
        if (!tank->map_vis) {
            goto error;
        }
        tk_debug("malloc %d(B) for tank(%s)->map_vis, items %d\n", map_vis_bytes, tank->name, map_vis_bytes/sizeof(tk_uint32_t));
#define CLEAN_TANK_MAP_VIS(tank) \
do{ \
    if (tank->map_vis) { \
        memset(tank->map_vis, 0, map_vis_bytes); \
    } \
}while(0)
        CLEAN_TANK_MAP_VIS(tank);
    }
    tank->position = pos;
    tank->angle_deg = angle_deg;
    SET_FLAG(tank, flags, TANK_ALIVE);
    // tank->basic_color = (void *)((TANK_ROLE_SELF == tank->role) ? ID2COLORPTR(TK_BLUE) : ID2COLORPTR(TK_RED));
    tank->health = tank->max_health = (TANK_ROLE_SELF == tank->role) ? 500 : 250;
    tank->speed = TANK_INIT_SPEED;
    tank->max_shell_num = DEFAULT_TANK_SHELLS_MAX_NUM;
    tank->current_grid = (Grid){-1, -1};
    calculate_tank_outline(&tank->position, TANK_LENGTH, TANK_WIDTH+4, calc_corrected_angle_deg(tank->angle_deg), &tank->practical_outline); // see handle_key()
    TAILQ_INIT(&tank->shell_list);

    if (TANK_ROLE_SELF == tank->role) {
        tk_shared_game_state.my_tank = tank;
    }
    lock(&tk_shared_game_state.spinlock);
    TAILQ_INSERT_HEAD(&tk_shared_game_state.tank_list, tank, chain);
    unlock(&tk_shared_game_state.spinlock);

    init_spinlock(&tank->spinlock);
    tk_debug("create a tank(name:%s, id:%lu, total size:%luB, ExplodeEffect's size: %luB) %p success, total tank num %u\n", 
        tank->name, tank->id, sizeof(Tank), sizeof(tank->explode_effect), tank, tank_num+1);
    return tank;

error:
    tk_debug("Error: create tank %s failed\n", name);
    if (tank->map_vis) {
        free(tank->map_vis);
    }
    if (tank->steps_to_escape) {
        free(tank->steps_to_escape);
    }
    if (tank) {
        free(tank);
    }
    return NULL;
}

void delete_tank(Tank *tank, int dereference) {
    Shell *shell = NULL;
    Shell *tmp = NULL;
    tk_uint8_t shell_num = 0;
    Tank *t1 = NULL, *t2 = NULL;

    if (!tank) {
        return;
    }
    if (TANK_ROLE_SELF == tank->role) {
        tk_shared_game_state.my_tank = NULL;
    }
    if (dereference) {
        TAILQ_FOREACH_SAFE(t1, &tk_shared_game_state.tank_list, chain, t2) {
            if (t1 != tank) {
                continue;
            }
            lock(&tk_shared_game_state.spinlock);
            TAILQ_REMOVE(&tk_shared_game_state.tank_list, t1, chain);
            unlock(&tk_shared_game_state.spinlock);
        }
    }
    TAILQ_FOREACH_SAFE(shell, &tank->shell_list, chain, tmp) {
        lock(&tank->spinlock);
        TAILQ_REMOVE(&tank->shell_list, shell, chain);
        unlock(&tank->spinlock);
        delete_shell(shell, 0);
        shell_num++;
    }
    tk_debug("tank(%p, id:%lu) %s(flags:%lu, score:%u, health:%u) is deleted, and free %u shells\n", 
        tank, (tank)->id, (tank)->name, (tank)->flags, (tank)->score, (tank)->health, shell_num);
    id_pool_release(tk_idpool, tank->id);
    tank->id = 0;
    destroy_spinlock(&tank->spinlock);
    if (tank->map_vis) {
        free(tank->map_vis);
    }
    if (tank->steps_to_escape) {
        free(tank->steps_to_escape);
    }
    free(tank);
}

void init_game_state() {
    memset(&tk_shared_game_state, 0, sizeof(tk_shared_game_state));
    TAILQ_INIT(&tk_shared_game_state.tank_list);
    maze_generate(&tk_shared_game_state.maze);
    print_maze_walls(&tk_shared_game_state.maze);
    tk_shared_game_state.blocks = get_block_positions(&tk_shared_game_state.maze, &tk_shared_game_state.blocks_num);
    // for (int i=0; i<tk_shared_game_state.blocks_num; i++) {
    //     printf("[(%f,%f),(%f,%f)], ", tk_shared_game_state.blocks[i].start.x, tk_shared_game_state.blocks[i].start.y, 
    //         tk_shared_game_state.blocks[i].end.x, tk_shared_game_state.blocks[i].end.y);
    //     printf("\n");
    // }
    init_spinlock(&tk_shared_game_state.spinlock);
}

void cleanup_game_state() {
    Tank *tank = NULL;
    Tank *tmp = NULL;
    tk_uint8_t tank_num = 0;

    TAILQ_FOREACH_SAFE(tank, &tk_shared_game_state.tank_list, chain, tmp) {
        lock(&tk_shared_game_state.spinlock);
        TAILQ_REMOVE(&tk_shared_game_state.tank_list, tank, chain);
        unlock(&tk_shared_game_state.spinlock);
        delete_tank(tank, 0);
        tank_num++;
    }
    if (tk_shared_game_state.blocks) {
        free(tk_shared_game_state.blocks);
    }
    tk_shared_game_state.blocks_num = 0;
    destroy_spinlock(&tk_shared_game_state.spinlock);
    tk_debug("total %u tanks are all freed\n", tank_num);
}

Point get_line_center(const Point *p1, const Point *p2) {
    Point center;
    center.x = ((p1->x + p2->x) / 2);
    center.y = ((p1->y + p2->y) / 2);
    return center;
}

// 计算从给定点沿着指定方向移动指定距离后的新坐标
Point move_point(Point start, tk_float32_t direction, tk_float32_t distance) {
    /*North: direction == 0*/
	if (direction < 0) {
        direction = 0;
    }
    if (direction > 360) {
        direction = 360;
    }
    direction = 360 - direction + 90;
	if (direction >= 360) {
        direction -= 360;
    }

	// 将角度转换为弧度
    tk_float32_t direction_rad = direction * M_PI / 180.0f;
    // 计算新坐标
    Point end;
    end.x = start.x + distance * cos(direction_rad);
    end.y = start.y - distance * sin(direction_rad);
    return end;
}

// 绕指定点pivot旋转一个点point
Point rotate_point(const Point *point, tk_float32_t angle, const Point *pivot) {
    if (0 == angle) {
        return (Point){point->x, point->y};
    }
	/*对于一个点 (x, y) 绕中心点 (cx, cy) 旋转 θ 角度后的新坐标 (x', y') 为：
	x' = cx + (x - cx) * cosθ - (y - cy) * sinθ
	y' = cy + (x - cx) * sinθ + (y - cy) * cosθ*/
    // 平移到原点
    tk_float32_t translated_x = point->x - pivot->x;
    tk_float32_t translated_y = point->y - pivot->y;
    /* 计算旋转矩阵 R = [ cosθ  -sinθ ]
					  [ sinθ   cosθ ]
	所需的三角函数值 */
    tk_float32_t cos_angle = cosf(angle);
    tk_float32_t sin_angle = sinf(angle);
    // 应用旋转矩阵：R·([x,y]^T)
    tk_float32_t rotated_x = translated_x * cos_angle - translated_y * sin_angle;
    tk_float32_t rotated_y = translated_x * sin_angle + translated_y * cos_angle;
    // 平移回原位置
    return (Point){rotated_x + pivot->x, rotated_y + pivot->y};
}

// 计算坦克实体的轮廓边界
void calculate_tank_outline(const Point *center, tk_float32_t width, tk_float32_t height, tk_float32_t angle_deg, Rectangle *rect) {
    tk_float32_t angle = angle_deg * (M_PI / 180.0f);  // 将角度转换为弧度
    uint8_t i = 0;
    // 未旋转时矩形的四个顶点坐标
    Point points[4] = {
        {center->x - (width/2), center->y - (height/2)},
        {center->x + (width/2) + 6, center->y - (height/2)},
        {center->x + (width/2) + 6, center->y + (height/2)},
		{center->x - (width/2), center->y + (height/2)}
    }; //+6是炮管延伸出来的长度
	Point *new_points = (Point *)rect;

    // 旋转每个顶点
    for (i = 0; i < 4; i++) {
        new_points[i] = rotate_point(&(points[i]), angle, center);
    }
}

Grid get_grid_by_tank_position(Point *pos) {
    float x = (pos->x - tk_maze_offset.x) / GRID_SIZE;
    float y = (pos->y - tk_maze_offset.y) / GRID_SIZE;
    if (x < 0) {
        x -= 1;
    }
    if (y < 0) {
        y -= 1;
    }
    return (Grid){x, y};
}
#define get_grid_by_shell_position get_grid_by_tank_position

void swap_two_grid(Grid *grid1, Grid *grid2) {
    Grid t;
    t = *grid2;
    *grid2 = *grid1;
    *grid1 = t;
}

// 获取网格四个角的位置坐标（pos_dir位置方位：0：左上角、1：右上角、2：左下角、3：右下角）
Point get_pos_by_grid(Grid *grid, int pos_dir) {
    if (pos_dir == 0) {
        return (Point){grid->x * GRID_SIZE + tk_maze_offset.x, grid->y * GRID_SIZE + tk_maze_offset.y};
    } else if (pos_dir == 1) {
        return (Point){(grid->x+1) * GRID_SIZE + tk_maze_offset.x, grid->y * GRID_SIZE + tk_maze_offset.y};
    } else if (pos_dir == 2) {
        return (Point){grid->x * GRID_SIZE + tk_maze_offset.x, (grid->y+1) * GRID_SIZE + tk_maze_offset.y};
    } else {
        return (Point){(grid->x+1) * GRID_SIZE + tk_maze_offset.x, (grid->y+1) * GRID_SIZE + tk_maze_offset.y};
    }
}

Point get_center_pos_by_grid(Grid *grid) {
    Point pos0 = get_pos_by_grid(grid, 0);
    Point pos1 = get_pos_by_grid(grid, 3);

    return (Point){(pos0.x + pos1.x) / 2, (pos0.y + pos1.y) / 2};
}

/*判断坦克是否靠近指定网格的中心点*/
int is_tank_near_grid_center(Tank *tank, Grid *grid) {
    Point center = get_center_pos_by_grid(grid);
    if ((abs((tank->position.x - center.x)) <= 10) && (abs((tank->position.y - center.y)) <= 10)) {
        return 1;
    }
    return 0;
}

// 计算斜率，角度输入范围 0~360（正北为0°，顺时针增加）
double calculate_slope(double theta_degrees) {
    // 转换为数学标准角度（正东为0°，逆时针增加）
    double alpha_degrees = 90.0 - theta_degrees;

    // 处理垂直情况（0°或180°，斜率不存在）
    if (fabs(fmod(theta_degrees, 180.0)) < 1e-6) {
        return INFINITY; // 或 NAN，表示斜率不存在
    }

    // 转换为弧度并计算 tan
    double alpha_radians = alpha_degrees * (M_PI / 180.0);
    double slope = tan(alpha_radians);

    // 处理浮点精度问题（如 tan(90°) 可能返回极大值）
    if (fabs(slope) > 1e10) {
        return INFINITY; // 视为垂直线
    }

    return slope;
}
/*
int main() {
    double theta;
    printf("请输入角度（0~360°，正北为0°）：");
    scanf("%lf", &theta);

    double slope = calculate_slope(theta);

    if (isinf(slope)) {
        printf("斜率不存在（垂直线）\n");
    } else {
        printf("斜率 k = %lf\n", slope);
    }

    return 0;
}
*/

// 给定pos起点、方向(角度)，计算从起点射出去的线与pos所在网格的交点，以及确认下一个网格
void get_ray_intersection_dot_with_grid(Ray_Intersection_Dot_Info *info) {
    Point p;
    Grid g;
    double k0, k1;
    if(info->angle_deg >= 360) {
        info->angle_deg -= 360;
    }
    tk_debug_internal(DEBUG_SIGHT_LINE, "get_ray_intersection_dot_with_grid: start(%f,%f), angle_deg(%f), grid(%d,%d)\n", 
        POS(info->start_point), info->angle_deg, info->current_grid.x, info->current_grid.y);
    if (info->angle_deg == 0) { // 射线垂直向上
        info->k = INFINITY;
        p = get_pos_by_grid(&info->current_grid, 0);
        info->intersection_dot = (Point){info->start_point.x, p.y};
        if (info->current_grid.y == 0) { // 当前网格是最上一层网格，必定反射，射线转而垂直向下
            info->next_grid = info->current_grid;
            info->reflect_angle_deg = 180;
        } else {
            g.x = info->current_grid.x;
            g.y = info->current_grid.y-1;
            if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与上一层网格之间打通，射线射入上一层网格中
                info->next_grid = g;
            } else { // 当前网格与上一层网格之间未打通，垂直反射
                info->next_grid = info->current_grid;
                info->reflect_angle_deg = 180;
            }
        }
        tk_debug_internal(DEBUG_SIGHT_LINE, "result(0): intersection(%f,%f), next_grid(%d,%d), k(%f), reflect_angle_deg(%f)\n", 
            POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, info->reflect_angle_deg);
    } else if (info->angle_deg == 90) { // 射线水平向右
        info->k = 0;
        p = get_pos_by_grid(&info->current_grid, 1);
        info->intersection_dot = (Point){p.x, info->start_point.y};
        if ((info->current_grid.x+1) == HORIZON_GRID_NUMBER) { // 当前网格是最右一列网格，必定反射，射线转而水平向左
            info->next_grid = info->current_grid;
            info->reflect_angle_deg = 270;
        } else {
            g.x = info->current_grid.x+1;
            g.y = info->current_grid.y;
            if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与右侧网格之间打通，射线射入右侧网格中
                info->next_grid = g;
            } else { // 当前网格与右侧网格之间未打通，水平反射
                info->next_grid = info->current_grid;
                info->reflect_angle_deg = 270;
            }
        }
        tk_debug_internal(DEBUG_SIGHT_LINE, "result(1): intersection(%f,%f), next_grid(%d,%d), k(%f), reflect_angle_deg(%f)\n", 
            POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, info->reflect_angle_deg);
    } else if (info->angle_deg == 180) { // 射线垂直向下
        info->k = INFINITY;
        p = get_pos_by_grid(&info->current_grid, 3);
        info->intersection_dot = (Point){info->start_point.x, p.y};
        if ((info->current_grid.y+1) == VERTICAL_GRID_NUMBER) { // 当前网格是最下一层网格，必定反射，射线转而垂直向上
            info->next_grid = info->current_grid;
            info->reflect_angle_deg = 0;
        } else {
            g.x = info->current_grid.x;
            g.y = info->current_grid.y+1;
            if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与下一层网格之间打通，射线射入下一层网格中
                info->next_grid = g;
            } else { // 当前网格与下一层网格之间未打通，垂直反射
                info->next_grid = info->current_grid;
                info->reflect_angle_deg = 0;
            }
        }
        tk_debug_internal(DEBUG_SIGHT_LINE, "result(2): intersection(%f,%f), next_grid(%d,%d), k(%f), reflect_angle_deg(%f)\n", 
            POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, info->reflect_angle_deg);
    } else if (info->angle_deg == 270) {
        info->k = 0;
        p = get_pos_by_grid(&info->current_grid, 2);
        info->intersection_dot = (Point){p.x, info->start_point.y};
        if (info->current_grid.x == 0) { // 当前网格是最左一列网格，必定反射，射线转而水平向右
            info->next_grid = info->current_grid;
            info->reflect_angle_deg = 90;
        } else {
            g.x = info->current_grid.x-1;
            g.y = info->current_grid.y;
            if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与左侧网格之间打通，射线射入左侧网格中
                info->next_grid = g;
            } else { // 当前网格与左侧网格之间未打通，水平反射
                info->next_grid = info->current_grid;
                info->reflect_angle_deg = 90;
            }
        }
        tk_debug_internal(DEBUG_SIGHT_LINE, "result(3): intersection(%f,%f), next_grid(%d,%d), k(%f), reflect_angle_deg(%f)\n", 
            POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, info->reflect_angle_deg);
    } else {
        info->k = k1 = calculate_slope(info->angle_deg);
        if ((info->angle_deg > 0) && (info->angle_deg < 90)) { // k1 > 0
            p = get_pos_by_grid(&info->current_grid, 1);
            k0 = (info->start_point.y - p.y) / (p.x - info->start_point.x);
            if (k1 > k0) { // 射线与当前网格的上边框相交
                info->intersection_dot = (Point){info->start_point.x+((info->start_point.y-p.y)/k1), p.y};
                if (info->current_grid.y == 0) { // 与最上层墙壁发生镜面反射
                    info->next_grid = info->current_grid;
                    info->reflect_angle_deg = 180 - info->angle_deg;
                } else {
                    g.x = info->current_grid.x;
                    g.y = info->current_grid.y-1;
                    if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与上一层网格之间打通，射线射入上一层网格中
                        info->next_grid = g;
                    } else { // 当前网格与上一层网格之间未打通，镜面反射
                        info->next_grid = info->current_grid;
                        info->reflect_angle_deg = 180 - info->angle_deg;
                    }
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(4): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            } else if (k1 < k0) { // 射线与当前网格的右边框相交
                info->intersection_dot = (Point){p.x, info->start_point.y-(k1*(p.x-info->start_point.x))};
                if ((info->current_grid.x+1) == HORIZON_GRID_NUMBER) { // 与最右侧墙壁发生镜面反射
                    info->next_grid = info->current_grid;
                    info->reflect_angle_deg = 360 - info->angle_deg;
                } else {
                    g.x = info->current_grid.x+1;
                    g.y = info->current_grid.y;
                    if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与右侧网格之间打通，射线射入右侧网格中
                        info->next_grid = g;
                    } else { // 当前网格与右侧网格之间未打通，镜面反射
                        info->next_grid = info->current_grid;
                        info->reflect_angle_deg = 360 - info->angle_deg;
                    }
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(5): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            } else { // 正好射入右上角
                int connect1, connect2; // 与上侧/右侧网格的连通性
                if (info->current_grid.y == 0) {
                    connect1 = 0;
                } else {
                    g.x = info->current_grid.x;
                    g.y = info->current_grid.y-1;
                    connect1 = is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g);
                }
                if ((info->current_grid.x+1) == HORIZON_GRID_NUMBER) {
                    connect2 = 0;
                } else {
                    g.x = info->current_grid.x+1;
                    g.y = info->current_grid.y;
                    connect2 = is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g);
                }
                if (!connect1 || !connect2) { // 右上角两边存在至少一堵墙，则终止，否则直接射入右上角对角相连的网格
                    info->terminate_flag = 1;
                } else {
                    info->intersection_dot = get_pos_by_grid(&info->current_grid, 1);
                    info->next_grid.x = info->current_grid.x+1;
                    info->next_grid.y = info->current_grid.y-1;
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(1-0): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            }
        } else if ((info->angle_deg > 90) && (info->angle_deg < 180)) { // k1 < 0
            p = get_pos_by_grid(&info->current_grid, 3);
            k0 = -((p.y - info->start_point.y) / (p.x - info->start_point.x));
            if (k1 < k0) { // 射线与当前网格的下边框相交
                info->intersection_dot = (Point){info->start_point.x+((info->start_point.y-p.y)/k1), p.y};
                if ((info->current_grid.y+1) == VERTICAL_GRID_NUMBER) { // 与最下层墙壁发生镜面反射
                    info->next_grid = info->current_grid;
                    info->reflect_angle_deg = 180 - info->angle_deg;
                } else {
                    g.x = info->current_grid.x;
                    g.y = info->current_grid.y+1;
                    if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与下一层网格之间打通，射线射入下一层网格中
                        info->next_grid = g;
                    } else { // 当前网格与下一层网格之间未打通，镜面反射
                        info->next_grid = info->current_grid;
                        info->reflect_angle_deg = 180 - info->angle_deg;
                    }
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(6): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            } else if (k1 > k0) { // 射线与当前网格的右边框相交
                info->intersection_dot = (Point){p.x, info->start_point.y+(k1*(info->start_point.x-p.x))};
                if ((info->current_grid.x+1) == HORIZON_GRID_NUMBER) { // 与最右侧墙壁发生镜面反射
                    info->next_grid = info->current_grid;
                    info->reflect_angle_deg = 360 - info->angle_deg;
                } else {
                    g.x = info->current_grid.x+1;
                    g.y = info->current_grid.y;
                    if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与右侧网格之间打通，射线射入右侧网格中
                        info->next_grid = g;
                    } else { // 当前网格与右侧网格之间未打通，镜面反射
                        info->next_grid = info->current_grid;
                        info->reflect_angle_deg = 360 - info->angle_deg;
                    }
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(7): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            } else { // 正好射入右下角
                int connect1, connect2; // 与下侧/右侧网格的连通性
                if ((info->current_grid.y+1) == VERTICAL_GRID_NUMBER) {
                    connect1 = 0;
                } else {
                    g.x = info->current_grid.x;
                    g.y = info->current_grid.y+1;
                    connect1 = is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g);
                }
                if ((info->current_grid.x+1) == HORIZON_GRID_NUMBER) {
                    connect2 = 0;
                } else {
                    g.x = info->current_grid.x+1;
                    g.y = info->current_grid.y;
                    connect2 = is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g);
                }
                if (!connect1 || !connect2) { // 右下角两边存在至少一堵墙，则终止，否则直接射入右下角对角相连的网格
                    info->terminate_flag = 1;
                } else {
                    info->intersection_dot = get_pos_by_grid(&info->current_grid, 3);
                    info->next_grid.x = info->current_grid.x+1;
                    info->next_grid.y = info->current_grid.y+1;
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(1-1): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            }
        } else if ((info->angle_deg > 180) && (info->angle_deg < 270)) { // k1 > 0
            p = get_pos_by_grid(&info->current_grid, 2);
            k0 = (p.y - info->start_point.y) / (info->start_point.x - p.x);
            if (k1 > k0) { // 射线与当前网格的下边框相交
                info->intersection_dot = (Point){info->start_point.x-((p.y-info->start_point.y)/k1), p.y};
                if ((info->current_grid.y+1) == VERTICAL_GRID_NUMBER) { // 与最下层墙壁发生镜面反射
                    info->next_grid = info->current_grid;
                    info->reflect_angle_deg = 540 - info->angle_deg;
                } else {
                    g.x = info->current_grid.x;
                    g.y = info->current_grid.y+1;
                    if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与下一层网格之间打通，射线射入下一层网格中
                        info->next_grid = g;
                    } else { // 当前网格与下一层网格之间未打通，镜面反射
                        info->next_grid = info->current_grid;
                        info->reflect_angle_deg = 540 - info->angle_deg;
                    }
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(8): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            } else if (k1 < k0) { // 射线与当前网格的左边框相交
                info->intersection_dot = (Point){p.x, info->start_point.y+(k1*(info->start_point.x-p.x))};
                if (info->current_grid.x == 0) { // 与最左侧墙壁发生镜面反射
                    info->next_grid = info->current_grid;
                    info->reflect_angle_deg = 360 - info->angle_deg;
                } else {
                    g.x = info->current_grid.x-1;
                    g.y = info->current_grid.y;
                    if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与左侧网格之间打通，射线射入左侧网格中
                        info->next_grid = g;
                    } else { // 当前网格与左侧网格之间未打通，镜面反射
                        info->next_grid = info->current_grid;
                        info->reflect_angle_deg = 360 - info->angle_deg;
                    }
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(9): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            } else { // 正好射入左下角
                int connect1, connect2; // 与下侧/左侧网格的连通性
                if ((info->current_grid.y+1) == VERTICAL_GRID_NUMBER) {
                    connect1 = 0;
                } else {
                    g.x = info->current_grid.x;
                    g.y = info->current_grid.y+1;
                    connect1 = is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g);
                }
                if (info->current_grid.x == 0) {
                    connect2 = 0;
                } else {
                    g.x = info->current_grid.x-1;
                    g.y = info->current_grid.y;
                    connect2 = is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g);
                }
                if (!connect1 || !connect2) { // 左下角两边存在至少一堵墙，则终止，否则直接射入左下角对角相连的网格
                    info->terminate_flag = 1;
                } else {
                    info->intersection_dot = get_pos_by_grid(&info->current_grid, 2);
                    info->next_grid.x = info->current_grid.x-1;
                    info->next_grid.y = info->current_grid.y+1;
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(1-2): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            }
        } else if ((info->angle_deg > 270) && (info->angle_deg < 360)) { // k1 < 0
            p = get_pos_by_grid(&info->current_grid, 0);
            k0 = -((info->start_point.y - p.y) / (info->start_point.x - p.x));
            if (k1 < k0) { // 射线与当前网格的上边框相交
                info->intersection_dot = (Point){info->start_point.x-((p.y-info->start_point.y)/k1), p.y};
                if (info->current_grid.y == 0) { // 与最上层墙壁发生镜面反射
                    info->next_grid = info->current_grid;
                    info->reflect_angle_deg = 540 - info->angle_deg;
                } else {
                    g.x = info->current_grid.x;
                    g.y = info->current_grid.y-1;
                    if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与上一层网格之间打通，射线射入上一层网格中
                        info->next_grid = g;
                    } else { // 当前网格与上一层网格之间未打通，镜面反射
                        info->next_grid = info->current_grid;
                        info->reflect_angle_deg = 540 - info->angle_deg;
                    }
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(10): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            } else if (k1 > k0) { // 射线与当前网格的左边框相交
                info->intersection_dot = (Point){p.x, info->start_point.y-(k1*(p.x-info->start_point.x))};
                if (info->current_grid.x == 0) { // 与最左侧墙壁发生镜面反射
                    info->next_grid = info->current_grid;
                    info->reflect_angle_deg = 360 - info->angle_deg;
                } else {
                    g.x = info->current_grid.x-1;
                    g.y = info->current_grid.y;
                    if (is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g)) { // 当前网格与左侧网格之间打通，射线射入左侧网格中
                        info->next_grid = g;
                    } else { // 当前网格与左侧网格之间未打通，镜面反射
                        info->next_grid = info->current_grid;
                        info->reflect_angle_deg = 360 - info->angle_deg;
                    }
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(11): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            } else { // 正好射入左上角
                int connect1, connect2; // 与上侧/左侧网格的连通性
                if (info->current_grid.y == 0) {
                    connect1 = 0;
                } else {
                    g.x = info->current_grid.x;
                    g.y = info->current_grid.y-1;
                    connect1 = is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g);
                }
                if (info->current_grid.x == 0) {
                    connect2 = 0;
                } else {
                    g.x = info->current_grid.x-1;
                    g.y = info->current_grid.y;
                    connect2 = is_two_grids_connected(&tk_shared_game_state.maze, &info->current_grid, &g);
                }
                if (!connect1 || !connect2) { // 左上角两边存在至少一堵墙，则终止，否则直接射入左上角对角相连的网格
                    info->terminate_flag = 1;
                } else {
                    info->intersection_dot = get_pos_by_grid(&info->current_grid, 0);
                    info->next_grid.x = info->current_grid.x-1;
                    info->next_grid.y = info->current_grid.y-1;
                }
                tk_debug_internal(DEBUG_SIGHT_LINE, "result(1-3): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
                    POS(info->intersection_dot), info->next_grid.x, info->next_grid.y, info->k, k0, info->reflect_angle_deg);
            }
        }
    }
}

#define DOT_IS_ON_LINE    0
#define DOT_LEFT_OF_LINE  1
#define DOT_RIGHT_OF_LINE 2

// 判断点C与线段（A和B是线段上的两个点）的关系：点在线左侧、点在线右侧、点在线上
int get_point_position_with_line(Point *C, Point *line_A, Point *line_B) {
    Vector2 AB = (Vector2){line_B->x - line_A->x, line_B->y - line_A->y};
    Vector2 AC = (Vector2){C->x - line_A->x, C->y - line_A->y};
    int cross = (AB.x*AC.y) - (AC.x*AB.y);

    if (cross > 0) {
        return DOT_RIGHT_OF_LINE; // GUI窗口坐标系和正常的坐标系正好相反，因此此处调换了LEFT与RIGHT
    } else if (cross < 0) {
        return DOT_LEFT_OF_LINE;
    } else {
        return DOT_IS_ON_LINE;
    }
}

// 判断两个位置之间是否穿过墙壁（1：穿过墙壁存在碰撞，0：反之未穿过墙壁无碰撞）
static int is_two_pos_transfer_through_wall(Point *pos1, Point *pos2) {
    Grid grid1 = get_grid_by_tank_position(pos1);
    Grid grid2 = get_grid_by_tank_position(pos2);
    Grid grid3, grid4;
    Point dot0;
    int t = 0;
    Point *p = NULL;

    // tk_debug_internal(1, "grid1:(%d,%d), grid2:(%d,%d)\n", grid1.x, grid1.y, grid2.x, grid2.y);
    if (!is_grid_valid(&grid1) || !is_grid_valid(&grid2)) { // invalid意味着网格已超出地图可视区域范围，也就是穿越了最外层一圈墙壁
        return 1;
    }

    int relationship = is_two_grids_adjacent(&grid1, &grid2); // 两个位置之间的网格关系：相邻、同一网格、对角、无关
    if (MAZE_SAME_GRID == relationship) { // 必无碰撞
        return 0;
    } else if (MAZE_ADJACENT_GRID == relationship) { // 两个网格相邻并之间打通，则与墙壁必无碰撞，否则必碰撞
        if (is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid2)) {
            return 0;
        }
        return 1;
    } else if (MAZE_DIAGONAL_GRID == relationship) { // 两个pos分别位于对角线相连的两个网格中，两个pos直连线可能碰撞墙壁也可能不碰撞
        if (((grid1.x-grid2.x > 0) && (grid1.y-grid2.y > 0)) || ((grid2.x-grid1.x > 0) && (grid2.y-grid1.y > 0))) { /* 对角线：\ */
            if (grid1.x-grid2.x > 0) {
                /*grid2,grid3     grid1,grid3
                  grid4,grid1  => grid4,grid2*/
                swap_two_grid(&grid1, &grid2);
                p = pos1;
                pos1 = pos2;
                pos2 = p;
            } else {
                /*grid1,grid3
                  grid4,grid2*/
            }
            grid3 = (Grid){grid2.x, grid1.y};
            grid4 = (Grid){grid1.x, grid2.y};
            dot0 = get_pos_by_grid(&grid1, 3);
            t = get_point_position_with_line(&dot0, pos1, pos2);
            if (t == DOT_LEFT_OF_LINE) { // dot0在线pos1-pos2的上方
                if (!is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid4) 
                    || !is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid4)) {
                    tk_debug_internal(DEBUG_TANK_COLLISION, ">1 | dot0(%f,%f), pos1(%f,%f), pos2(%f,%f), %d, %d, grid1:(%d,%d), grid2:(%d,%d), grid3:(%d,%d), grid4:(%d,%d)\n", 
                        POS(dot0), POSPTR(pos1), POSPTR(pos2), 
                        is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid4), 
                        is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid4), 
                        grid1.x, grid1.y, grid2.x, grid2.y, grid3.x, grid3.y, grid4.x, grid4.y);
                    return 1;
                }
                return 0;
            } else if (t == DOT_RIGHT_OF_LINE) { // dot0在线pos1-pos2的下方
                if (!is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid3) 
                    || !is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid3)) {
                    tk_debug_internal(DEBUG_TANK_COLLISION, ">2 | dot0(%f,%f), pos1(%f,%f), pos2(%f,%f), %d, %d, grid1:(%d,%d), grid2:(%d,%d), grid3:(%d,%d), grid4:(%d,%d)\n", 
                        POS(dot0), POSPTR(pos1), POSPTR(pos2), 
                        is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid3), 
                        is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid3), 
                        grid1.x, grid1.y, grid2.x, grid2.y, grid3.x, grid3.y, grid4.x, grid4.y);
                    return 1;
                }
                return 0;
            } else { // dot0在pos1-pos2线上
                return 0;
            }
        } else { /* 对角线：/ */
            if (grid1.x-grid2.x > 0) {
                /*grid3,grid1
                  grid2,grid4*/
            } else {
                /*grid3,grid2     grid3,grid1
                  grid1,grid4  => grid2,grid4*/
                swap_two_grid(&grid1, &grid2);
                p = pos1;
                pos1 = pos2;
                pos2 = p;
            }
            grid3 = (Grid){grid2.x, grid1.y};
            grid4 = (Grid){grid1.x, grid2.y};
            dot0 = get_pos_by_grid(&grid3, 3);
            t = get_point_position_with_line(&dot0, pos1, pos2);
            if (t == DOT_LEFT_OF_LINE) { // dot0在线pos1-pos2的下方
                if (!is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid3) 
                    || !is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid3)) {
                    tk_debug_internal(DEBUG_TANK_COLLISION, ">3 | dot0(%f,%f), pos1(%f,%f), pos2(%f,%f), %d, %d, grid1:(%d,%d), grid2:(%d,%d), grid3:(%d,%d), grid4:(%d,%d)\n", 
                        POS(dot0), POSPTR(pos1), POSPTR(pos2), 
                        is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid3), 
                        is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid3), 
                        grid1.x, grid1.y, grid2.x, grid2.y, grid3.x, grid3.y, grid4.x, grid4.y);
                    return 1;
                }
                return 0;
            } else if (t == DOT_RIGHT_OF_LINE) { // dot0在线pos1-pos2的上方
                if (!is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid4) 
                    || !is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid4)) {
                    tk_debug_internal(DEBUG_TANK_COLLISION, ">4 | dot0(%f,%f), pos1(%f,%f), pos2(%f,%f), %d, %d, grid1:(%d,%d), grid2:(%d,%d), grid3:(%d,%d), grid4:(%d,%d)\n", 
                        POS(dot0), POSPTR(pos1), POSPTR(pos2), 
                        is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid4), 
                        is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid4), 
                        grid1.x, grid1.y, grid2.x, grid2.y, grid3.x, grid3.y, grid4.x, grid4.y);
                    return 1;
                }
                return 0;
            } else { // dot0在pos1-pos2线上
                return 0;
            }
        }
    } else { // 对于坦克轮廓的四个顶点来说，其中任意两个顶点的relationship位置关系只能是上面三种，因此can't be here，但要真出现异常走到这里就认为是存在碰撞
        tk_debug("Error: %s detect MAZE_UNRELATED_GRID(%d)? grid1:(%d,%d), grid2:(%d,%d)\n", __func__, 
            relationship, grid1.x, grid1.y, grid2.x, grid2.y);
        return 1;
    }

    return 0; // can't be here
}

extern bool is_my_tank_collide_with_other_tanks(Tank *my_tank, Rectangle *newest_outline);

void handle_key(Tank *tank, KeyValue *key_value) {
    tk_float32_t backward_dir = 0;
    Rectangle outline;

    if (!tank || !key_value) return;
    if ((key_value->mask) == 0) return;
    Point new_position = tank->position;
    tk_float32_t new_angle_deg = tank->angle_deg;

    // print_key_value(key_value);
    if (TST_FLAG(key_value, mask, TK_KEY_W_ACTIVE)) {
        new_position = move_point(tank->position, tank->angle_deg, tank->speed);
    }
    if (TST_FLAG(key_value, mask, TK_KEY_S_ACTIVE)) {
        backward_dir = tank->angle_deg + 180;
        if (backward_dir > 360) {
            backward_dir -= 360;
        }
        new_position = move_point(tank->position, backward_dir, tank->speed);
    }
#define PER_TICK_ANGLE_DEG_CHANGE 5
    if (TST_FLAG(key_value, mask, TK_KEY_A_ACTIVE)) {
        new_angle_deg += 360;
        new_angle_deg -= PER_TICK_ANGLE_DEG_CHANGE;
        if (new_angle_deg >= 360) {
            new_angle_deg -= 360;
        }
    }
    if (TST_FLAG(key_value, mask, TK_KEY_D_ACTIVE)) {
        new_angle_deg += PER_TICK_ANGLE_DEG_CHANGE;
		if (new_angle_deg >= 360) {
            new_angle_deg -= 360;
        }
    }

    // 计算坦克轮廓边界
    tk_float32_t corrected_angle_deg = new_angle_deg; // 角度修正：tank->angle_deg:=0指向正北，而矩阵旋转变换 角度0代表指向正东，
    // 因此要实现“tank->angle_deg:=0指向正北”意味着旋转变换的旋转角度是270。see render_tank()
    if (corrected_angle_deg < 0) {
        corrected_angle_deg = 0;
    }
    if (corrected_angle_deg > 360) {
        corrected_angle_deg = 360;
    }
    corrected_angle_deg += 270;
    if (corrected_angle_deg >= 360) {
        corrected_angle_deg -= 360;
    }
    // 基于最新位置、角度计算出坦克轮廓边界，然后对轮廓做碰撞检查，没有发生碰撞，则本次移动、旋转动作是合法的，否则不合法需撤回本次移动
    calculate_tank_outline(&new_position, TANK_LENGTH, TANK_WIDTH+4, corrected_angle_deg, &outline); //+4是履带额外的宽度

    // // 计算坦克轮廓矩形四个角所处的网格
    // Grid grid0 = get_grid_by_tank_position(&outline.righttop);
    // Grid grid1 = get_grid_by_tank_position(&outline.rightbottom);
    // Grid grid2 = get_grid_by_tank_position(&outline.leftbottom);
    // Grid grid3 = get_grid_by_tank_position(&outline.lefttop);
    // tk_debug_internal(DEBUG_CONTROL_THREAD_DETAIL, "grid:(%d, %d)/%d, (%d, %d)/%d, (%d, %d)/%d, (%d, %d)/%d\n", 
    //     POS(grid0), grid_id(&grid0), POS(grid1), grid_id(&grid1), POS(grid2), grid_id(&grid2), POS(grid3), grid_id(&grid3));

    tank->collision_flag &= 0xF0;
    CLR_FLAG(tank, collision_flag, COLLISION_WITH_TANK);
    if (is_two_pos_transfer_through_wall(&outline.righttop, &outline.rightbottom)) {
        tk_debug_internal(DEBUG_TANK_COLLISION, "前方发生碰撞\n");
        SET_FLAG(tank, collision_flag, COLLISION_FRONT);
    } else if (is_two_pos_transfer_through_wall(&outline.lefttop, &outline.righttop)) {
        tk_debug_internal(DEBUG_TANK_COLLISION, "左侧发生碰撞\n");
        SET_FLAG(tank, collision_flag, COLLISION_LEFT);
    } else if (is_two_pos_transfer_through_wall(&outline.rightbottom, &outline.leftbottom)) {
        tk_debug_internal(DEBUG_TANK_COLLISION, "右侧发生碰撞\n");
        SET_FLAG(tank, collision_flag, COLLISION_RIGHT);
    } else if (is_two_pos_transfer_through_wall(&outline.leftbottom, &outline.lefttop)) {
        tk_debug_internal(DEBUG_TANK_COLLISION, "后方发生碰撞\n");
        SET_FLAG(tank, collision_flag, COLLISION_BACK);
    } else { // 未与墙壁发生碰撞
        if (!is_my_tank_collide_with_other_tanks(tank, &outline)) { //未与地图上的其他坦克发生碰撞
            tank->position = new_position;
            tank->angle_deg = new_angle_deg;
            tank->practical_outline = outline;
        } else {
            SET_FLAG(tank, collision_flag, COLLISION_WITH_TANK);
        }
    }
    tank->outline = outline; // 将可能发生了碰撞的最新轮廓绘制出来用于debug
}

Shell* create_shell_for_tank(Tank *tank) {
    if (!tank) return NULL;
    if ((tank->health <= 0) || !TST_FLAG(tank, flags, TANK_ALIVE)) return NULL;

    Shell *shell = NULL;
    tk_uint8_t shell_num = 0;
    TAILQ_FOREACH(shell, &tank->shell_list, chain) {
        shell_num++;
    }
    if (shell_num >= tank->max_shell_num) {
        tk_debug("Warn: can't create more shells(%u>=MAX/%u) for tank(%s)\n", shell_num, tank->max_shell_num, tank->name);
        SET_FLAG(tank, flags, TANK_FORBID_SHOOT);
        return NULL;
    }
    CLR_FLAG(tank, flags, TANK_FORBID_SHOOT);

    shell = malloc(sizeof(Shell));
    if (!shell) {
        goto error;
    }
    memset(shell, 0, sizeof(shell));

    shell->id = id_pool_allocate(tk_idpool);
    if (!shell->id) {
        tk_debug("Error: %s id_pool_allocate failed\n", __func__);
        goto error;
    }
    shell->position = get_line_center(&tank->practical_outline.righttop, &tank->practical_outline.rightbottom);
    shell->angle_deg = tank->angle_deg;
    shell->speed = SHELL_INIT_SPEED;
    shell->tank_owner = (void*)tank;
    shell->ttl = SHELL_COLLISION_MAX_NUM;
    lock(&tank->spinlock);
    TAILQ_INSERT_HEAD(&tank->shell_list, shell, chain);
    unlock(&tank->spinlock);
    shell_num += 1;
    tk_debug("create a shell(id:%lu) %p at (%f,%f) for tank(%s) success, the tank now has %u shells\n", shell->id, shell, 
        POS(shell->position), tank->name, shell_num);
    if (shell_num >= tank->max_shell_num) {
        SET_FLAG(tank, flags, TANK_FORBID_SHOOT);
    }
    return shell;
error:
    tk_debug("Error: create shell for tank(%s) failed\n", tank->name);
    if (shell) {
        free(shell);
    }
    return NULL;
}

void delete_shell(Shell *shell, int dereference) {
    if (!shell) return;

    Shell *s = NULL, *t = NULL;
    tk_debug("shell(%p, id:%lu) of tank(%s) is deleted\n", shell, shell->id, ((Tank*)(shell->tank_owner))->name);
    if (dereference) {
        TAILQ_FOREACH_SAFE(s, &((Tank*)(shell->tank_owner))->shell_list, chain, t) {
            if (s != shell) {
                continue;
            }
            lock(&((Tank*)(shell->tank_owner))->spinlock);
            TAILQ_REMOVE(&((Tank*)(shell->tank_owner))->shell_list, s, chain);
            unlock(&((Tank*)(shell->tank_owner))->spinlock);
        }
    }
    shell->tank_owner = NULL;
    id_pool_release(tk_idpool, shell->id);
    shell->id = 0;
    free(shell);
}

double calculate_tan(double angle_degrees) {
    angle_degrees = fmod(angle_degrees, 360);  // 确保角度在 0~360 范围内
    if (angle_degrees == 90.0 || angle_degrees == 270.0) {
        printf("tan(%.1f°) is undefined (infinity)\n", angle_degrees);
        return INFINITY;  // 返回无穷大
    }
    double angle_radians = angle_degrees * (M_PI / 180.0);
    return tan(angle_radians);
}
/*int main() {
    double angles[] = {0, 30, 45, 60, 90, 180, 270, 360};
    for (int i = 0; i < 8; i++) {
        double tan_val = calculate_tan(angles[i]);
        if (!isinf(tan_val)) {  // 检查是否为无穷大
            printf("tan(%.1f°) = %.6f\n", angles[i], tan_val);
        }
    }
    return 0;
}*/

#define SHELL_COLLISION_EPSILON 0.1
int is_equal_double(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

/*判断某位置是否在网格的边框线上，上下浮动范围_float，返回0表示不在边框上，返回1表示在上方边框线，2表示右，3表示下，4表示左*/
#define POS_AT_LEFT_BORDER   0x01
#define POS_AT_RIGHT_BORDER  0x02
#define POS_AT_TOP_BORDER    0x04
#define POS_AT_BOTTOM_BORDER 0x08
tk_uint8_t is_pos_at_grid_border_line(Point *pos, Grid *grid, tk_uint8_t _float) {
    int top_border = grid->y * GRID_SIZE + tk_maze_offset.y;
    int right_border = (grid->x+1) * GRID_SIZE + tk_maze_offset.x;
    int bottom_border = (grid->y+1) * GRID_SIZE + tk_maze_offset.y;
    int left_border = (grid->x) * GRID_SIZE + tk_maze_offset.x;
    tk_uint8_t flag = 0;

    if ((left_border >= (pos->x-_float)) && (left_border <= (pos->x+_float))) {
        SET_FLAG2(flag, POS_AT_LEFT_BORDER);
    }
    if ((right_border >= (pos->x-_float)) && (right_border <= (pos->x+_float))) {
        SET_FLAG2(flag, POS_AT_RIGHT_BORDER);
    }
    if ((top_border >= (pos->y-_float)) && (top_border <= (pos->y+_float))) {
        SET_FLAG2(flag, POS_AT_TOP_BORDER);
    }
    if ((bottom_border >= (pos->y-_float)) && (bottom_border <= (pos->y+_float))) {
        SET_FLAG2(flag, POS_AT_BOTTOM_BORDER);
    }
    return flag;
}

extern Tank* is_my_shell_collide_with_other_tanks(Shell *shell);

#define FINETUNE_SHELL_RADIUS_LENGTH (SHELL_RADIUS_LENGTH+0)

void update_one_shell_movement_position(Shell *shell, int need_to_detect_collision_with_tank) {
    Point new_pos;
    Grid current_grid;
    tk_float32_t wall_x = 0;
    tk_float32_t wall_y = 0;
    Point p;
    Grid next_grid;
    tk_float32_t new_angle_deg = 0;
    tk_uint8_t collide_wall_x = 0; // 水平墙壁
    tk_uint8_t collide_wall_y = 0; // 垂直墙壁
    Tank *tank = NULL;
    tk_uint16_t blood_loss = 0;
    double k = 0;
    tk_float32_t x = 0, y = 0;
    Grid t;
    tk_uint8_t pos_flag = 0;
    tk_uint8_t f0 = 0, f1 = 0;

    if (0 == shell->ttl) {
        return;
    }
    new_pos = move_point(shell->position, shell->angle_deg, shell->speed);
    next_grid = current_grid = get_grid_by_shell_position(&shell->position);
    new_angle_deg = shell->angle_deg;
    // goto out;

    if (shell->angle_deg == 0) { // 前进方向为上（正北）
        p = get_pos_by_grid(&current_grid, 0);
        wall_x = p.y;
        if ((new_pos.y-FINETUNE_SHELL_RADIUS_LENGTH) <= wall_x) { // 可能与上方墙壁发生碰撞，之所以是可能，是因为还未判断上方是否真的存在墙壁
            next_grid.y -= 1;
            if ((next_grid.y < 0) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) { // 上方存在墙壁
                new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                new_angle_deg = 180; // 反弹方向
            } else {
                pos_flag = is_pos_at_grid_border_line(&shell->position, &current_grid, FINETUNE_SHELL_RADIUS_LENGTH);
                if (TST_FLAG2(pos_flag, POS_AT_LEFT_BORDER)) {
                    if (current_grid.x > 0) {
                        t = (Grid){current_grid.x-1, next_grid.y};
                        if (!is_two_grids_connected(&tk_shared_game_state.maze, &next_grid, &t)) {
                            new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                            new_angle_deg = 180; // 反弹方向
                        }
                    }
                } else if (TST_FLAG2(pos_flag, POS_AT_RIGHT_BORDER)) {
                    if (current_grid.x < (HORIZON_GRID_NUMBER-1)) {
                        t = (Grid){current_grid.x+1, next_grid.y};
                        if (!is_two_grids_connected(&tk_shared_game_state.maze, &next_grid, &t)) {
                            new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                            new_angle_deg = 180; // 反弹方向
                        }
                    }
                } else {
                    goto out;
                }
            }
        } else {
            goto out;
        }
    } else if (shell->angle_deg == 90) {
        p = get_pos_by_grid(&current_grid, 1);
        wall_y = p.x;
        if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) >= wall_y) {
            next_grid.x += 1;
            if ((next_grid.x >= HORIZON_GRID_NUMBER) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                new_angle_deg = 270;
            } else {
                pos_flag = is_pos_at_grid_border_line(&shell->position, &current_grid, FINETUNE_SHELL_RADIUS_LENGTH);
                if (TST_FLAG2(pos_flag, POS_AT_TOP_BORDER)) {
                    if (current_grid.y > 0) {
                        t = (Grid){next_grid.x, current_grid.y-1};
                        if (!is_two_grids_connected(&tk_shared_game_state.maze, &next_grid, &t)) {
                            new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                            new_angle_deg = 270;
                        }
                    }
                } else if (TST_FLAG2(pos_flag, POS_AT_BOTTOM_BORDER)) {
                    if (current_grid.y < (VERTICAL_GRID_NUMBER-1)) {
                        t = (Grid){next_grid.x, current_grid.y+1};
                        if (!is_two_grids_connected(&tk_shared_game_state.maze, &next_grid, &t)) {
                            new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                            new_angle_deg = 270;
                        }
                    }
                } else {
                    goto out;
                }
            }
        } else {
            goto out;
        }
    } else if (shell->angle_deg == 180) {
        p = get_pos_by_grid(&current_grid, 3);
        wall_x = p.y;
        if ((new_pos.y+FINETUNE_SHELL_RADIUS_LENGTH) >= wall_x) {
            next_grid.y += 1;
            if ((next_grid.y >= VERTICAL_GRID_NUMBER) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                new_angle_deg = 0;
            } else {
                pos_flag = is_pos_at_grid_border_line(&shell->position, &current_grid, FINETUNE_SHELL_RADIUS_LENGTH);
                if (TST_FLAG2(pos_flag, POS_AT_LEFT_BORDER)) {
                    if (current_grid.x > 0) {
                        t = (Grid){current_grid.x-1, next_grid.y};
                        if (!is_two_grids_connected(&tk_shared_game_state.maze, &next_grid, &t)) {
                            new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                            new_angle_deg = 0;
                        }
                    }
                } else if (TST_FLAG2(pos_flag, POS_AT_RIGHT_BORDER)) {
                    if (current_grid.x < (HORIZON_GRID_NUMBER-1)) {
                        t = (Grid){current_grid.x+1, next_grid.y};
                        if (!is_two_grids_connected(&tk_shared_game_state.maze, &next_grid, &t)) {
                            new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                            new_angle_deg = 0;
                        }
                    }
                } else {
                    goto out;
                }
            }
        } else {
            goto out;
        }
    } else if (shell->angle_deg == 270) {
        p = get_pos_by_grid(&current_grid, 2);
        wall_y = p.x;
        if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) <= wall_y) {
            next_grid.x -= 1;
            if ((next_grid.x < 0) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                new_angle_deg = 90;
            } else {
                pos_flag = is_pos_at_grid_border_line(&shell->position, &current_grid, FINETUNE_SHELL_RADIUS_LENGTH);
                if (TST_FLAG2(pos_flag, POS_AT_TOP_BORDER)) {
                    if (current_grid.y > 0) {
                        t = (Grid){next_grid.x, current_grid.y-1};
                        if (!is_two_grids_connected(&tk_shared_game_state.maze, &next_grid, &t)) {
                            new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                            new_angle_deg = 90;
                        }
                    }
                } else if (TST_FLAG2(pos_flag, POS_AT_BOTTOM_BORDER)) {
                    if (current_grid.y < (VERTICAL_GRID_NUMBER-1)) {
                        t = (Grid){next_grid.x, current_grid.y+1};
                        if (!is_two_grids_connected(&tk_shared_game_state.maze, &next_grid, &t)) {
                            new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                            new_angle_deg = 90;
                        }
                    }
                } else {
                    goto out;
                }
            }
        } else {
            goto out;
        }
    } else {
        if ((shell->angle_deg > 0) && (shell->angle_deg < 90)) { // 前进方向为右上角
            p = get_pos_by_grid(&current_grid, 1);
            wall_x = p.y;
            wall_y = p.x;
            collide_wall_x = 0; // 是否碰撞上墙壁
            if ((new_pos.y-FINETUNE_SHELL_RADIUS_LENGTH) <= wall_x) {
                next_grid.y -= 1;
                if ((next_grid.y < 0) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                    collide_wall_x = 1;
                }
            }
            next_grid = current_grid;
            collide_wall_y = 0; // 是否碰撞右墙壁
            if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) >= wall_y) {
                next_grid.x += 1;
                if ((next_grid.x >= HORIZON_GRID_NUMBER) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                    collide_wall_y = 1;
                }
            }
            /*特殊情况*/
            if (!collide_wall_x && !collide_wall_y) {
                if (((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) >= wall_y) && (new_pos.y-FINETUNE_SHELL_RADIUS_LENGTH) <= wall_x) {
                    tk_uint8_t hit_opposite_wall_x = !is_two_grids_connected(&tk_shared_game_state.maze, 
                        &(Grid){current_grid.x+1, current_grid.y}, &(Grid){current_grid.x+1, current_grid.y-1});
                    tk_uint8_t hit_opposite_wall_y = !is_two_grids_connected(&tk_shared_game_state.maze, 
                        &(Grid){current_grid.x, current_grid.y-1}, &(Grid){current_grid.x+1, current_grid.y-1});
                    if (!hit_opposite_wall_x && !hit_opposite_wall_y) {
                        goto out;
                    }
                    k = calculate_slope(shell->angle_deg); //k·x + y - (k·x0 + y0) = 0
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞检测1：k=%f(%u,%u), pos(%f,%f), wallxy(%f,%f)\n", 
                        k, hit_opposite_wall_x, hit_opposite_wall_y, POS(shell->position), wall_x, wall_y);
                    f0 = f1 = 0;
                    if (hit_opposite_wall_x) { //opposite_wall_x存在的意思
                        x = ((shell->position.y + k*shell->position.x) - wall_x) / k;
                        if (is_equal_double(x, wall_y, SHELL_COLLISION_EPSILON)) {
                            f0 = 1;
                            tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞1：x=%f\n", x);
                            collide_wall_y = 1; //与wall_y垂直墙壁发生碰撞
                        } else if ((x > wall_y) && (x < (wall_y+GRID_SIZE))) {
                            if ((shell->position.y-FINETUNE_SHELL_RADIUS_LENGTH) < wall_x) {
                                f0 = 5;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞5：x=%f\n", x);
                                collide_wall_y = 1;
                            } else {
                                f0 = 2;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞2：x=%f\n", x);
                                collide_wall_x = 1; //与wall_x水平墙壁发生碰撞
                            }
                        } else if ((x+(FINETUNE_SHELL_RADIUS_LENGTH+1)) >= wall_y) {
                            if ((shell->position.x+(FINETUNE_SHELL_RADIUS_LENGTH+1)) >= wall_y) {
                                f0 = 4;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞4：x=%f\n", x);
                                collide_wall_x = 1;
                            } else {
                                f0 = 3;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞3：x=%f\n", x);
                                collide_wall_y = 1;
                            }
                        }
                    }
                    if (hit_opposite_wall_y) { //opposite_wall_y存在的意思
                        y = (shell->position.y + k*shell->position.x) - k*wall_y;
                        if (is_equal_double(y, wall_x, SHELL_COLLISION_EPSILON)) {
                            f1 = 1;
                            tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞1：y=%f\n", y);
                            collide_wall_x = 1;
                        } else if ((y < wall_x) && (y > (wall_x-GRID_SIZE))) {
                            if ((shell->position.x+FINETUNE_SHELL_RADIUS_LENGTH) > wall_y) {
                                f1 = 5;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞5：y=%f\n", y);
                                collide_wall_x = 1;
                            } else {
                                f1 = 2;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞2：y=%f\n", y);
                                collide_wall_y = 1;
                            }
                        } else if ((y-(FINETUNE_SHELL_RADIUS_LENGTH+1)) <= wall_x) {
                            if ((shell->position.y-(FINETUNE_SHELL_RADIUS_LENGTH+1)) <= wall_x) {
                                f1 = 4;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞4：y=%f\n", y);
                                collide_wall_y = 1;
                            } else {
                                f1 = 3;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞3：y=%f\n", y);
                                collide_wall_x = 1;
                            }
                        }
                    }
                    if ((f0 == 4) && (f1 == 2)) {
                        collide_wall_x = 0;
                    } else if ((f0 == 2) && (f1 == 4)) {
                        collide_wall_y = 0;
                    }
                }
            }
            tk_debug_internal(DEBUG_SHELL_COLLISION, "current_grid(%d,%d), new_pos(%f,%f), wall_x(%f, wall_y(%f)), collide_wall(%d,%d)\n", current_grid.x, current_grid.y, 
                POS(new_pos), wall_x, wall_y, collide_wall_x, collide_wall_y);
            if (collide_wall_x && collide_wall_y) { // 碰撞墙角
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(假设上) | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x + ((shell->position.y - (new_pos.y)) / calculate_tan(90 - shell->angle_deg));
                new_angle_deg = 180 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) > wall_y) {
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(实际右) | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                        shell->angle_deg, wall_y, POS(new_pos));
                    new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.y = shell->position.y - ((new_pos.x - shell->position.x) / calculate_tan(shell->angle_deg));
                    new_angle_deg = 360 - shell->angle_deg;
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                } else if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) == wall_y) { // 刚好触碰两面墙壁，原路反弹回去
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(刚好触碰右上两面墙壁)\n");
                    new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_angle_deg = shell->angle_deg + 180;
                    if (new_angle_deg >= 360) {
                        new_angle_deg -= 360;
                    }
                }
            } else if (collide_wall_x && !collide_wall_y) { // 碰撞(上)单面墙壁
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰(上)单面墙壁 | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x + ((shell->position.y - (new_pos.y)) / calculate_tan(90 - shell->angle_deg));
                new_angle_deg = 180 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else if (!collide_wall_x && collide_wall_y) { // 碰撞(右)单面墙壁
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰(右)单面墙壁 | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_y, POS(new_pos));
                new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.y = shell->position.y - ((new_pos.x - shell->position.x) / calculate_tan(shell->angle_deg));
                new_angle_deg = 360 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else { // 无碰撞
                goto out;
            }
        } else if ((shell->angle_deg > 90) && (shell->angle_deg < 180)) {
            p = get_pos_by_grid(&current_grid, 3);
            wall_x = p.y;
            wall_y = p.x;
            collide_wall_x = 0;
            if ((new_pos.y+FINETUNE_SHELL_RADIUS_LENGTH) >= wall_x) {
                next_grid.y += 1;
                if ((next_grid.y >= VERTICAL_GRID_NUMBER) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                    collide_wall_x = 1;
                }
            }
            next_grid = current_grid;
            collide_wall_y = 0;
            if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) >= wall_y) {
                next_grid.x += 1;
                if ((next_grid.x >= HORIZON_GRID_NUMBER) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                    collide_wall_y = 1;
                }
            }
            /*特殊情况*/
            if (!collide_wall_x && !collide_wall_y) {
                if (((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) >= wall_y) && (new_pos.y+FINETUNE_SHELL_RADIUS_LENGTH) >= wall_x) {
                    tk_uint8_t hit_opposite_wall_x = !is_two_grids_connected(&tk_shared_game_state.maze, 
                        &(Grid){current_grid.x+1, current_grid.y}, &(Grid){current_grid.x+1, current_grid.y+1});
                    tk_uint8_t hit_opposite_wall_y = !is_two_grids_connected(&tk_shared_game_state.maze, 
                        &(Grid){current_grid.x, current_grid.y+1}, &(Grid){current_grid.x+1, current_grid.y+1});
                    if (!hit_opposite_wall_x && !hit_opposite_wall_y) {
                        goto out;
                    }
                    k = calculate_slope(shell->angle_deg); //k·x + y - (k·x0 + y0) = 0
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞检测2：k=%f(%u,%u), pos(%f,%f), wallxy(%f,%f)\n", 
                        k, hit_opposite_wall_x, hit_opposite_wall_y, POS(shell->position), wall_x, wall_y);
                    f0 = f1 = 0;
                    if (hit_opposite_wall_x) {
                        x = ((shell->position.y + k*shell->position.x) - wall_x) / k;
                        if (is_equal_double(x, wall_y, SHELL_COLLISION_EPSILON)) {
                            f0 = 1;
                            tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞1：x=%f\n", x);
                            collide_wall_y = 1;
                        } else if ((x > wall_y) && (x < (wall_y+GRID_SIZE))) {
                            if ((shell->position.y+FINETUNE_SHELL_RADIUS_LENGTH) > wall_x) {
                                f0 = 5;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞5：x=%f\n", x);
                                collide_wall_y = 1;
                            } else {
                                f0 = 2;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞2：x=%f\n", x);
                                collide_wall_x = 1;
                            }
                        } else if ((x+(FINETUNE_SHELL_RADIUS_LENGTH+1)) >= wall_y) {
                            if ((shell->position.x+(FINETUNE_SHELL_RADIUS_LENGTH+1)) >= wall_y) {
                                f0 = 4;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞4：x=%f\n", x);
                                collide_wall_x = 1;
                            } else {
                                f0 = 3;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞3：x=%f\n", x);
                                collide_wall_y = 1;
                            }
                        }
                    }
                    if (hit_opposite_wall_y) {
                        y = (shell->position.y + k*shell->position.x) - k*wall_y;
                        if (is_equal_double(y, wall_x, SHELL_COLLISION_EPSILON)) {
                            f1 = 1;
                            tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞1：y=%f\n", y);
                            collide_wall_x = 1;
                        } else if ((y > wall_x) && (y < (wall_x+GRID_SIZE))) {
                            if ((shell->position.x+FINETUNE_SHELL_RADIUS_LENGTH) > wall_y) {
                                f1 = 5;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞5：y=%f\n", y);
                                collide_wall_x = 1;
                            } else {
                                f1 = 2;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞2：y=%f\n", y);
                                collide_wall_y = 1;
                            }
                        } else if ((y+(FINETUNE_SHELL_RADIUS_LENGTH+1)) >= wall_x) {
                            if ((shell->position.y+(FINETUNE_SHELL_RADIUS_LENGTH+1)) >= wall_x) {
                                f1 = 4;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞4：y=%f\n", y);
                                collide_wall_y = 1;
                            } else {
                                f1 = 3;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞3：y=%f\n", y);
                                collide_wall_x = 1;
                            }
                        }
                    }
                    if ((f0 == 4) && (f1 == 2)) {
                        collide_wall_x = 0;
                    } else if ((f0 == 2) && (f1 == 4)) {
                        collide_wall_y = 0;
                    }
                }
            }
            tk_debug_internal(DEBUG_SHELL_COLLISION, "current_grid(%d,%d), new_pos(%f,%f), wall_x(%f, wall_y(%f)), collide_wall(%d,%d)\n", current_grid.x, current_grid.y, 
                POS(new_pos), wall_x, wall_y, collide_wall_x, collide_wall_y);
            if (collide_wall_x && collide_wall_y) {
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(假设下) | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x + ((new_pos.y - shell->position.y) / calculate_tan(shell->angle_deg - 90));
                new_angle_deg = 180 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) > wall_y) {
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(实际右) | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                        shell->angle_deg, wall_y, POS(new_pos));
                    new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.y = shell->position.y + ((new_pos.x - shell->position.x) / calculate_tan(180 - shell->angle_deg));
                    new_angle_deg = 360 - shell->angle_deg;
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                } else if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) == wall_y) {
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(刚好触碰右下两面墙壁)\n");
                    new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_angle_deg = shell->angle_deg + 180;
                    if (new_angle_deg >= 360) {
                        new_angle_deg -= 360;
                    }
                }
            } else if (collide_wall_x && !collide_wall_y) { // 碰撞(下)单面墙壁
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰(下)单面墙壁 | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x + ((new_pos.y - shell->position.y) / calculate_tan(shell->angle_deg - 90));
                new_angle_deg = 180 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else if (!collide_wall_x && collide_wall_y) { // 碰撞(右)单面墙壁
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰(右)单面墙壁 | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_y, POS(new_pos));
                new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.y = shell->position.y + ((new_pos.x - shell->position.x) / calculate_tan(180 - shell->angle_deg));
                new_angle_deg = 360 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else {
                goto out;
            }
        } else if ((shell->angle_deg > 180) && (shell->angle_deg < 270)) {
            p = get_pos_by_grid(&current_grid, 2);
            wall_x = p.y;
            wall_y = p.x;
            collide_wall_x = 0;
            if ((new_pos.y+FINETUNE_SHELL_RADIUS_LENGTH) >= wall_x) {
                next_grid.y += 1;
                if ((next_grid.y >= VERTICAL_GRID_NUMBER) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                    collide_wall_x = 1;
                }
            }
            next_grid = current_grid;
            collide_wall_y = 0;
            if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) <= wall_y) {
                next_grid.x -= 1;
                if ((next_grid.x < 0) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                    collide_wall_y = 1;
                }
            }
            /*特殊情况*/
            if (!collide_wall_x && !collide_wall_y) {
                if (((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) <= wall_y) && (new_pos.y+FINETUNE_SHELL_RADIUS_LENGTH) >= wall_x) {
                    tk_uint8_t hit_opposite_wall_x = !is_two_grids_connected(&tk_shared_game_state.maze, 
                        &(Grid){current_grid.x-1, current_grid.y}, &(Grid){current_grid.x-1, current_grid.y+1});
                    tk_uint8_t hit_opposite_wall_y = !is_two_grids_connected(&tk_shared_game_state.maze, 
                        &(Grid){current_grid.x, current_grid.y+1}, &(Grid){current_grid.x-1, current_grid.y+1});
                    if (!hit_opposite_wall_x && !hit_opposite_wall_y) {
                        goto out;
                    }
                    k = calculate_slope(shell->angle_deg); //k·x + y - (k·x0 + y0) = 0
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞检测3：k=%f(%u,%u), pos(%f,%f), wallxy(%f,%f)\n", 
                        k, hit_opposite_wall_x, hit_opposite_wall_y, POS(shell->position), wall_x, wall_y);
                    f0 = f1 = 0;
                    if (hit_opposite_wall_x) {
                        x = ((shell->position.y + k*shell->position.x) - wall_x) / k;
                        if (is_equal_double(x, wall_y, SHELL_COLLISION_EPSILON)) {
                            f0 = 1;
                            tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞1：x=%f\n", x);
                            collide_wall_y = 1;
                        } else if ((x < wall_y) && (x > (wall_y-GRID_SIZE))) {
                            if ((shell->position.y+FINETUNE_SHELL_RADIUS_LENGTH) > wall_x) {
                                f0 = 5;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞5：x=%f\n", x);
                                collide_wall_y = 1;
                            } else {
                                f0 = 2;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞2：x=%f\n", x);
                                collide_wall_x = 1;
                            }
                        } else if ((x-(FINETUNE_SHELL_RADIUS_LENGTH+1)) <= wall_y) {
                            if ((shell->position.x-(FINETUNE_SHELL_RADIUS_LENGTH+1)) <= wall_y) {
                                f0 = 4;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞4：x=%f\n", x);
                                collide_wall_x = 1;
                            } else {
                                f0 = 3;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞3：x=%f\n", x);
                                collide_wall_y = 1;
                            }
                        }
                    }
                    if (hit_opposite_wall_y) {
                        y = (shell->position.y + k*shell->position.x) - k*wall_y;
                        if (is_equal_double(y, wall_x, SHELL_COLLISION_EPSILON)) {
                            f1 = 1;
                            tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞1：y=%f\n", y);
                            collide_wall_x = 1;
                        } else if ((y > wall_x) && (y < (wall_x+GRID_SIZE))) {
                            if ((shell->position.x-FINETUNE_SHELL_RADIUS_LENGTH) < wall_y) {
                                f1 = 5;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞5：y=%f\n", y);
                                collide_wall_x = 1;
                            } else {
                                f1 = 2;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞2：y=%f\n", y);
                                collide_wall_y = 1;
                            }
                        } else if ((y+(FINETUNE_SHELL_RADIUS_LENGTH+1) >= wall_x)) {
                            if ((shell->position.y+(FINETUNE_SHELL_RADIUS_LENGTH+1) >= wall_x)) {
                                f1 = 4;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞4：y=%f\n", y);
                                collide_wall_y = 1;
                            } else {
                                f1 = 3;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞3：y=%f\n", y);
                                collide_wall_x = 1;
                            }
                        }
                    }
                    if ((f0 == 4) && (f1 == 2)) {
                        collide_wall_x = 0;
                    } else if ((f0 == 2) && (f1 == 4)) {
                        collide_wall_y = 0;
                    }
                }
            }
            tk_debug_internal(DEBUG_SHELL_COLLISION, "current_grid(%d,%d), new_pos(%f,%f), wall_x(%f, wall_y(%f)), collide_wall(%d,%d)\n", current_grid.x, current_grid.y, 
                POS(new_pos), wall_x, wall_y, collide_wall_x, collide_wall_y);
            if (collide_wall_x && collide_wall_y) {
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(假设下) | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x - ((new_pos.y - shell->position.y) / calculate_tan(270 - shell->angle_deg));
                new_angle_deg = 540 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) < wall_y) {
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(实际左) | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                        shell->angle_deg, wall_y, POS(new_pos));
                    new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.y = shell->position.y + ((shell->position.x - new_pos.x) / calculate_tan(shell->angle_deg - 180));
                    new_angle_deg = 360 - shell->angle_deg;
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                } else if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) == wall_y) {
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(刚好触碰左下两面墙壁)\n");
                    new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_angle_deg = shell->angle_deg + 180;
                    if (new_angle_deg >= 360) {
                        new_angle_deg -= 360;
                    }
                }
            } else if (collide_wall_x && !collide_wall_y) { // 碰撞(下)单面墙壁
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰(下)单面墙壁 | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x - ((new_pos.y - shell->position.y) / calculate_tan(270 - shell->angle_deg));
                new_angle_deg = 540 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else if (!collide_wall_x && collide_wall_y) { // 碰撞(左)单面墙壁
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰(左)单面墙壁 | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_y, POS(new_pos));
                new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.y = shell->position.y + ((shell->position.x - new_pos.x) / calculate_tan(shell->angle_deg - 180));
                new_angle_deg = 360 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else {
                goto out;
            }
        } else if ((shell->angle_deg > 270) && (shell->angle_deg < 360)) {
            p = get_pos_by_grid(&current_grid, 0);
            wall_x = p.y;
            wall_y = p.x;
            collide_wall_x = 0;
            if ((new_pos.y-FINETUNE_SHELL_RADIUS_LENGTH) <= wall_x) {
                next_grid.y -= 1;
                if ((next_grid.y < 0) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                    collide_wall_x = 1;
                }
            }
            next_grid = current_grid;
            collide_wall_y = 0;
            if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) <= wall_y) {
                next_grid.x -= 1;
                if ((next_grid.x < 0) || (!is_two_grids_connected(&tk_shared_game_state.maze, &current_grid, &next_grid))) {
                    collide_wall_y = 1;
                }
            }
            /*特殊情况*/
            if (!collide_wall_x && !collide_wall_y) {
                if (((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) <= wall_y) && (new_pos.y-FINETUNE_SHELL_RADIUS_LENGTH) <= wall_x) {
                    tk_uint8_t hit_opposite_wall_x = !is_two_grids_connected(&tk_shared_game_state.maze, 
                        &(Grid){current_grid.x-1, current_grid.y}, &(Grid){current_grid.x-1, current_grid.y-1});
                    tk_uint8_t hit_opposite_wall_y = !is_two_grids_connected(&tk_shared_game_state.maze, 
                        &(Grid){current_grid.x, current_grid.y-1}, &(Grid){current_grid.x-1, current_grid.y-1});
                    if (!hit_opposite_wall_x && !hit_opposite_wall_y) {
                        goto out;
                    }
                    k = calculate_slope(shell->angle_deg); //k·x + y - (k·x0 + y0) = 0
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞检测4：k=%f(%u,%u), pos(%f,%f), wallxy(%f,%f)\n", 
                        k, hit_opposite_wall_x, hit_opposite_wall_y, POS(shell->position), wall_x, wall_y);
                    f0 = f1 = 0;
                    if (hit_opposite_wall_x) {
                        x = ((shell->position.y + k*shell->position.x) - wall_x) / k;
                        if (is_equal_double(x, wall_y, SHELL_COLLISION_EPSILON)) {
                            f0 = 1;
                            tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞1：x=%f\n", x);
                            collide_wall_y = 1;
                        } else if ((x < wall_y) && (x > (wall_y-GRID_SIZE))) {
                            if ((shell->position.y-FINETUNE_SHELL_RADIUS_LENGTH) < wall_x) {
                                f0 = 5;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞5：x=%f\n", x);
                                collide_wall_y = 1;
                            } else {
                                f0 = 2;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞2：x=%f\n", x);
                                collide_wall_x = 1;
                            }
                        } else if ((x-(FINETUNE_SHELL_RADIUS_LENGTH+1)) <= wall_y) {
                            if ((shell->position.x-(FINETUNE_SHELL_RADIUS_LENGTH+1)) <= wall_y) {
                                f0 = 4;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞4：x=%f\n", x);
                                collide_wall_x = 1;
                            } else {
                                f0 = 3;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞3：x=%f\n", x);
                                collide_wall_y = 1;
                            }
                        }
                    }
                    if (hit_opposite_wall_y) {
                        y = (shell->position.y + k*shell->position.x) - k*wall_y;
                        if (is_equal_double(y, wall_x, SHELL_COLLISION_EPSILON)) {
                            f1 = 1;
                            tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞1：y=%f\n", y);
                            collide_wall_x = 1;
                        } else if ((y < wall_x) && (y > (wall_x-GRID_SIZE))) {
                            if ((shell->position.x-FINETUNE_SHELL_RADIUS_LENGTH) < wall_y) {
                                f1 = 5;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞5：y=%f\n", y);
                                collide_wall_x = 1;
                            } else {
                                f1 = 2;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞2：y=%f\n", y);
                                collide_wall_y = 1;
                            }
                        } else if ((y-(FINETUNE_SHELL_RADIUS_LENGTH+1)) <= wall_x) {
                            if ((shell->position.y-(FINETUNE_SHELL_RADIUS_LENGTH+1)) <= wall_x) {
                                f1 = 4;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞4：y=%f\n", y);
                                collide_wall_y = 1;
                            } else {
                                f1 = 3;
                                tk_debug_internal(DEBUG_SHELL_COLLISION, "特殊碰撞3：y=%f\n", y);
                                collide_wall_x = 1;
                            }
                        }
                    }
                    if ((f0 == 4) && (f1 == 2)) {
                        collide_wall_x = 0;
                    } else if ((f0 == 2) && (f1 == 4)) {
                        collide_wall_y = 0;
                    }
                }
            }
            tk_debug_internal(DEBUG_SHELL_COLLISION, "current_grid(%d,%d), new_pos(%f,%f), wall_x(%f, wall_y(%f)), collide_wall(%d,%d)\n", current_grid.x, current_grid.y, 
                POS(new_pos), wall_x, wall_y, collide_wall_x, collide_wall_y);
            if (collide_wall_x && collide_wall_y) {
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(假设上) | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x - ((shell->position.y - (new_pos.y)) / calculate_tan(shell->angle_deg - 270));
                new_angle_deg = 540 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) < wall_y) {
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(实际左) | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                        shell->angle_deg, wall_y, POS(new_pos));
                    new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.y = shell->position.y - ((shell->position.x - new_pos.x) / calculate_tan(360 - shell->angle_deg));
                    new_angle_deg = 360 - shell->angle_deg;
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                } else if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) == wall_y) {
                    tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰墙角(刚好触碰左上两面墙壁)\n");
                    new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_angle_deg = shell->angle_deg + 180;
                    if (new_angle_deg >= 360) {
                        new_angle_deg -= 360;
                    }
                }
            } else if (collide_wall_x && !collide_wall_y) { // 碰撞(上)单面墙壁
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰(上)单面墙壁 | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x - ((shell->position.y - (new_pos.y)) / calculate_tan(shell->angle_deg - 270));
                new_angle_deg = 540 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else if (!collide_wall_x && collide_wall_y) { // 碰撞(左)单面墙壁
                tk_debug_internal(DEBUG_SHELL_COLLISION, "触碰(左)单面墙壁 | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                        shell->angle_deg, wall_y, POS(new_pos));
                new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.y = shell->position.y - ((shell->position.x - new_pos.x) / calculate_tan(360 - shell->angle_deg));
                new_angle_deg = 360 - shell->angle_deg;
                tk_debug_internal(DEBUG_SHELL_COLLISION, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else {
                goto out;
            }
        }
    }

    if (new_angle_deg != shell->angle_deg) { // 发生碰撞
        if (shell->ttl >= 1) {
            shell->ttl -= 1;
        }
    }
out: // 没有发生碰撞反弹直接out
    if (need_to_detect_collision_with_tank) {
        tank = is_my_shell_collide_with_other_tanks(shell);
        if (tank) {
            blood_loss = ((tk_float32_t)(shell->ttl <= 1 ? 1 : shell->ttl) / SHELL_COLLISION_MAX_NUM)*(50);
            if (tank->health >= blood_loss) {
                tank->health -= blood_loss; // 炮弹的威力随着反弹数量增加而减小，炮弹击中敌人最多消耗其50滴血
            } else {
                tank->health = 0;
            }
            shell->ttl = 0;
            SET_FLAG(tank, flags, TANK_IS_HIT_BY_ENEMY);
            if (tank->health <= 0) {
                tk_debug("坦克(%s)被%s的炮弹(ID:%u)击毁！\n", tank->name, ((Tank *)(shell->tank_owner))->name, shell->id);
                // delete_tank(tank, 1); //此时还不能立即destroy/free被击毁的坦克，因为爆炸特效的绘制需要一些时间，因此
                // 需要依赖定时器延迟删除坦克，当前是放在update_muggle_enemy_position()中去完成deaded坦克的删除释放
            } else {
                tk_debug("%s的炮弹(ID:%u)击中了坦克%s(掉血%u,剩余血量%u)\n", ((Tank *)(shell->tank_owner))->name, shell->id, tank->name, blood_loss, tank->health);
            }
            return;
        }
    }
    shell->position = new_pos;
    shell->angle_deg = new_angle_deg;
}

#if 0
// 判断两个点 (x1, y1) 和 (x2, y2) 是否足够接近（距离 < 1）
int is_near(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float distance_squared = dx * dx + dy * dy;  // 避免 sqrt 计算，直接比较平方
    return (distance_squared < 1.0f) ? 1 : 0;
}
#else
// 判断两点是否足够接近（|x1-x2|<1 && |y1-y2|<1）
int is_near(float x1, float y1, float x2, float y2) {
    float dx = fabsf(x2 - x1);  // x 方向绝对差值
    float dy = fabsf(y2 - y1);  // y 方向绝对差值
    return (dx < 0.6f && dy < 0.6f) ? 1 : 0;
}
#endif

// 更新炮弹移动状态
void update_all_shell_movement_position() {
    Tank *tank = NULL, *tt = NULL;
    Shell *shell = NULL, *ts = NULL;
    Point old_pos;

    TAILQ_FOREACH_SAFE(tank, &tk_shared_game_state.tank_list, chain, tt) {
        TAILQ_FOREACH_SAFE(shell, &tank->shell_list, chain, ts) {
            old_pos = shell->position;
            update_one_shell_movement_position(shell, 1);
            tk_debug_internal(DEBUG_CONTROL_THREAD_DETAIL, "shell %lu(tank %lu) move from (%f,%f) to (%f,%f)\n", 
                shell->id, tank->id, POS(old_pos), POS(shell->position));
            /*如果上次移动位置即将触碰墙壁，本次前进则会检测到碰撞，因此本次前进的步伐非常之微小，可以认为前后都处于同一位置，
            简单来说，正常一个位置只有一帧画面的话，那现在就变成两帧都在同一位置，会使得玩家观察到碰撞反弹处炮弹迟滞一段时间的现象，
            对于这种情况，需要再次执行前进动作*/
            if (is_near(POS(old_pos), POS(shell->position))) {
                update_one_shell_movement_position(shell, 0);
                tk_debug_internal(DEBUG_CONTROL_THREAD_DETAIL, "本次移动距离太小，再次移动！(%f,%f)=>(%f,%f)\n", POS(old_pos), POS(shell->position));
            }
            if (shell->ttl <= 0) { // shell is dead
                lock(&tank->spinlock);
                TAILQ_REMOVE(&tank->shell_list, shell, chain);
                unlock(&tank->spinlock);
                delete_shell(shell, 0);
                CLR_FLAG(tank, flags, TANK_FORBID_SHOOT);
            }
        }
    }
}

/*根据当前坦克位置，随机获取可以移动的方向（上：1，下：2，左：3，右：4），参数
no_backtracking为True表示禁止走回头路，譬如坦克从左侧来，然后又选择往左侧去，那就不许
不让走回头路虽然好一点，但是其他可选方向依然是随机选择，导致坦克经常限于某一局部区域移动，
如何更智能地选取方向？答案就是增加全局网格是否访问过的标记数据，优先选取尚未探访过的或访问量最少的网格方向*/
int get_movable_direction(Tank *tank, int no_backtracking) {
    Grid grid, next;
    int option[4] = {0}, option_num = 0; // 移动方向待选列表
    int from_dir = 0; // 坦克来的方向
    int i = 0;

    if (no_backtracking) {
        if ((tank->angle_deg > 45) && (tank->angle_deg < 135)) {
            from_dir = 3;
        } else if ((tank->angle_deg > 135) && (tank->angle_deg < 225)) {
            from_dir = 1;
        } else if ((tank->angle_deg > 225) && (tank->angle_deg < 315)) {
            from_dir = 4;
        } else if (((tank->angle_deg > 315) && (tank->angle_deg <= 360)) || ((tank->angle_deg >= 0) && (tank->angle_deg < 45))) {
            from_dir = 2;
        }
    }

    if (!tank) return 0;
    grid = get_grid_by_tank_position(&tank->position);
    if (grid.y > 0) {
        next.x = grid.x;
        next.y = grid.y - 1;
        if (is_two_grids_connected(&tk_shared_game_state.maze, &grid, &next)) {
            if ((no_backtracking && (from_dir != 1)) || (!no_backtracking)) {
                option[option_num++] = 1;
            }
        }
    }
    if ((grid.y + 1) < VERTICAL_GRID_NUMBER) {
        next.x = grid.x;
        next.y = grid.y + 1;
        if (is_two_grids_connected(&tk_shared_game_state.maze, &grid, &next)) {
            if ((no_backtracking && (from_dir != 2)) || (!no_backtracking)) {
                option[option_num++] = 2;
            }
        }
    }
    if (grid.x > 0) {
        next.x = grid.x - 1;
        next.y = grid.y;
        if (is_two_grids_connected(&tk_shared_game_state.maze, &grid, &next)) {
            if ((no_backtracking && (from_dir != 3)) || (!no_backtracking)) {
                option[option_num++] = 3;
            }
        }
    }
    if ((grid.x + 1) < HORIZON_GRID_NUMBER) {
        next.x = grid.x + 1;
        next.y = grid.y;
        if (is_two_grids_connected(&tk_shared_game_state.maze, &grid, &next)) {
            if ((no_backtracking && (from_dir != 4)) || (!no_backtracking)) {
                option[option_num++] = 4;
            }
        }
    }
    if (option_num == 0) {
        return 0; // 失败：没有找到可移动方向（只有在no_backtracking开启情况下才可能失败）
    }
    if (tank->map_vis) {
        int min_map_vis_val = 0;
        int min_map_vis_ind = 0;
        for (i=0; i<option_num; i++) {
            if (option[i] == 1) {
                next.x = grid.x;
                next.y = grid.y - 1;
            } else if (option[i] == 2) {
                next.x = grid.x;
                next.y = grid.y + 1;
            } else if (option[i] == 3) {
                next.x = grid.x - 1;
                next.y = grid.y;
            } else if (option[i] == 4) {
                next.x = grid.x + 1;
                next.y = grid.y;
            }
            if (!tank->map_vis[next.y][next.x]) { //从未访问过的网格
                tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "get_movable_direction %d(weight:0)\n", option[i]);
                return option[i];
            }
            //选取一个访问量最少的网格
            if ((min_map_vis_val == 0) || (tank->map_vis[next.y][next.x] < min_map_vis_val)) {
                min_map_vis_val = tank->map_vis[next.y][next.x];
                min_map_vis_ind = i;
            }
        }
        tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "get_movable_direction %d(weight:%u)\n", option[min_map_vis_ind], min_map_vis_val);
        return option[min_map_vis_ind];
    }
    return option[random_range(0, option_num-1)];
}

void save_steps_to_escape(Tank *tank, int index, int num, int direction) {
    int i = 0;
    if ((index < 0) || (index >= STEPS_TO_ESCAPE_NUM)) {
        return;
    }
    for (i=index; i<STEPS_TO_ESCAPE_NUM; i++) {
        if (num >= 16) {
            tank->steps_to_escape[i] = (15 << 4) | (direction & 0x0F);
            num -= 15;
        } else {
            tank->steps_to_escape[i] = (num << 4) | (direction & 0x0F);
            return;
        }
    }
}

void reset_rotation_direction_for_tank(Tank *tank, int index) {
    int movable_direction = 0;
    int t = 0;
    { // 怎么旋转？答：朝着四周没有墙壁的某个方向旋转，尽量避免往墙上撞
        movable_direction = get_movable_direction(tank, 1);
        if (!movable_direction) {
            movable_direction = get_movable_direction(tank, 0);
        }
        if (!movable_direction) {
            tank->steps_to_escape[index] = (random_range(6, 24) << 4) | (MOVE_RIGHT);
        } else {
            if (movable_direction == 1) {
                if (tank->angle_deg > 180) {
                    t = ((int)((360 - tank->angle_deg) / PER_TICK_ANGLE_DEG_CHANGE + 0.5));
                    tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向右旋转%d步以向上\n", t);
                    save_steps_to_escape(tank, index, t, MOVE_RIGHT);
                } else {
                    t = ((int)((tank->angle_deg) / PER_TICK_ANGLE_DEG_CHANGE + 0.5));
                    tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向左旋转%d步以向上\n", t);
                    save_steps_to_escape(tank, index, t, MOVE_LEFT);
                }
            } else if (movable_direction == 2) {
                if (tank->angle_deg > 180) {
                    t = ((int)((tank->angle_deg - 180) / PER_TICK_ANGLE_DEG_CHANGE + 0.5));
                    tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向左旋转%d步以向下\n", t);
                    save_steps_to_escape(tank, index, t, MOVE_LEFT);
                } else {
                    t = ((int)((180 - tank->angle_deg) / PER_TICK_ANGLE_DEG_CHANGE + 0.5));
                    tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向右旋转%d步以向下\n", t);
                    save_steps_to_escape(tank, index, t, MOVE_RIGHT);
                }
            } else if (movable_direction == 3) {
                if ((tank->angle_deg >= 90) && (tank->angle_deg <= 270)) {
                    t = ((int)((270 - tank->angle_deg) / PER_TICK_ANGLE_DEG_CHANGE + 0.5));
                    tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向右旋转%d步以向左\n", t);
                    save_steps_to_escape(tank, index, t, MOVE_RIGHT);
                } else {
                    if ((tank->angle_deg >= 0) && (tank->angle_deg < 90)) {
                        t = ((int)((90 + tank->angle_deg) / PER_TICK_ANGLE_DEG_CHANGE + 0.5));
                        tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向左旋转%d步以向左\n", t);
                        save_steps_to_escape(tank, index, t, MOVE_LEFT);
                    } else {
                        t = ((int)((tank->angle_deg - 270) / PER_TICK_ANGLE_DEG_CHANGE + 0.5));
                        tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向左旋转%d步以向左\n", t);
                        save_steps_to_escape(tank, index, t, MOVE_LEFT);
                    }
                }
            } else if (movable_direction == 4) {
                if ((tank->angle_deg >= 90) && (tank->angle_deg <= 270)) {
                    t = ((int)((tank->angle_deg - 90) / PER_TICK_ANGLE_DEG_CHANGE + 0.5));
                    tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向左旋转%d步以向右\n", t);
                    save_steps_to_escape(tank, index, t, MOVE_LEFT);
                } else {
                    if ((tank->angle_deg >= 0) && (tank->angle_deg < 90)) {
                        t = ((int)((90 - tank->angle_deg) / PER_TICK_ANGLE_DEG_CHANGE + 0.5));
                        tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向右旋转%d步以向右\n", t);
                        save_steps_to_escape(tank, index, t, MOVE_RIGHT);
                    } else {
                        t = ((int)((450 - tank->angle_deg) / PER_TICK_ANGLE_DEG_CHANGE + 0.5));
                        tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向左旋转%d步以向右\n", t);
                        save_steps_to_escape(tank, index, t, MOVE_RIGHT);
                    }
                }
            }
        }
    }
}

// 傻瓜坦克随机自由移动并发射炮弹
void update_muggle_enemy_position() {
    Tank *tank = NULL, *tt = NULL;
    tk_float32_t new_angle_deg = 0;
    int i = 0;
    Grid grid;
    int index = 0;

    TAILQ_FOREACH_SAFE(tank, &tk_shared_game_state.tank_list, chain, tt) {
        if ((tank->health <= 0) || !TST_FLAG(tank, flags, TANK_ALIVE)) {
            if (TST_FLAG(tank, flags, TANK_DEAD)) {
                delete_tank(tank, 1);
            }
            continue;
        }
        if (TANK_ROLE_ENEMY_MUGGLE == tank->role) {
            if (((tank->collision_flag << 4) != 0) || (TST_FLAG(tank, collision_flag, COLLISION_WITH_TANK))) { // 如果前进方向遇到阻塞，就尝试计划脱困
                tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "%s前行遇阻，尝试向后移动并旋转脱困(collision_flag:%u, position:(%f,%f), angle_deg:%f)\n", 
                    tank->name, tank->collision_flag, POS(tank->position), tank->angle_deg);
                /*设置脱困步骤*/
                for (i=0; i<STEPS_TO_ESCAPE_NUM; i++) {
                    tank->steps_to_escape[i] = 0;
                }
                tank->steps_to_escape[0] = (random_range(3, 6) << 4) | (MOVE_BACK);
                tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "计划向后%d步\n", tank->steps_to_escape[0] >> 4);
                reset_rotation_direction_for_tank(tank, 1);
                /*本次先向后退一步*/
                tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "%s向后移动\n", tank->name);
                tank->key_value_for_control.mask = 0;
                SET_FLAG(&(tank->key_value_for_control), mask, TK_KEY_S_ACTIVE);
                tank->steps_to_escape[0] = (((tank->steps_to_escape[0] >> 4) - 1) << 4) | (MOVE_BACK);
                handle_key(tank, &(tank->key_value_for_control));
                if (((tank->collision_flag << 4) != 0) || (TST_FLAG(tank, collision_flag, COLLISION_WITH_TANK))) { // 向后遇阻，重新调整脱困方案
                    if (random_range(0,1) == 1) {
                        tank->steps_to_escape[0] = (random_range(3, 6) << 4) | (MOVE_RIGHT);
                    } else {
                        tank->steps_to_escape[0] = (random_range(3, 6) << 4) | (MOVE_LEFT);
                    }
                }
            } else {
                for (i=0; i<STEPS_TO_ESCAPE_NUM; i++) {
                    if (tank->steps_to_escape[i] == 0) {
                        continue;
                    }
                    if (((tank->steps_to_escape[i] & 0x0F) == MOVE_FRONT) && ((tank->steps_to_escape[i] >> 4) > 0)) {
                        tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "%s向前移动\n", tank->name);
                        tank->key_value_for_control.mask = 0;
                        SET_FLAG(&(tank->key_value_for_control), mask, TK_KEY_W_ACTIVE);
                        tank->steps_to_escape[i] = (((tank->steps_to_escape[i] >> 4) - 1) << 4) | (MOVE_FRONT);
                        handle_key(tank, &(tank->key_value_for_control));
                        goto iter_next_tank;
                    } else if (((tank->steps_to_escape[i] & 0x0F) == MOVE_RIGHT) && ((tank->steps_to_escape[i] >> 4) > 0)) {
                        tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "%s向右移动(angle_deg:%f)\n", tank->name, tank->angle_deg);
                        tank->key_value_for_control.mask = 0;
                        SET_FLAG(&(tank->key_value_for_control), mask, TK_KEY_D_ACTIVE);
                        tank->steps_to_escape[i] = (((tank->steps_to_escape[i] >> 4) - 1) << 4) | (MOVE_RIGHT);
                        handle_key(tank, &(tank->key_value_for_control));
                        goto iter_next_tank;
                    } else if (((tank->steps_to_escape[i] & 0x0F) == MOVE_LEFT) && ((tank->steps_to_escape[i] >> 4) > 0)) {
                        tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "%s向左移动(angle_deg:%f)\n", tank->name, tank->angle_deg);
                        tank->key_value_for_control.mask = 0;
                        SET_FLAG(&(tank->key_value_for_control), mask, TK_KEY_A_ACTIVE);
                        tank->steps_to_escape[i] = (((tank->steps_to_escape[i] >> 4) - 1) << 4) | (MOVE_LEFT);
                        handle_key(tank, &(tank->key_value_for_control));
                        goto iter_next_tank;
                    } else if (((tank->steps_to_escape[i] & 0x0F) == MOVE_BACK) && ((tank->steps_to_escape[i] >> 4) > 0)) {
                        tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "%s向后移动\n", tank->name);
                        tank->key_value_for_control.mask = 0;
                        SET_FLAG(&(tank->key_value_for_control), mask, TK_KEY_S_ACTIVE);
                        tank->steps_to_escape[i] = (((tank->steps_to_escape[i] >> 4) - 1) << 4) | (MOVE_BACK);
                        handle_key(tank, &(tank->key_value_for_control));
                        goto iter_next_tank;
                    }
                }
                for (i=0; i<STEPS_TO_ESCAPE_NUM; i++) {
                    tank->steps_to_escape[i] = 0;
                }
                grid = get_grid_by_tank_position(&tank->position);
                if (!is_two_grids_the_same(&tank->current_grid, &grid)) {
                    tank->current_grid = grid;
                    CLR_FLAG(tank, flags, TANK_HAS_DECIDE_NEW_DIR_FOR_MUGGLE_ENEMY);
                    tank->map_vis[tank->current_grid.y][tank->current_grid.x] += 1;
                }
                if (!TST_FLAG(tank, flags, TANK_HAS_DECIDE_NEW_DIR_FOR_MUGGLE_ENEMY) && is_tank_near_grid_center(tank, &grid)) { // 这里near判断有点严格了（即如果坦克稍微走偏了就可能会被认为不靠近中心），不过也没关系
                    /*每当到达一个新的网格中心位置处，就需要重新决策前进方向*/
                    SET_FLAG(tank, flags, TANK_HAS_DECIDE_NEW_DIR_FOR_MUGGLE_ENEMY);
                    tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "%s重新选择方向(current_grid:(%d,%d))\n", tank->name, POS(tank->current_grid));
                    reset_rotation_direction_for_tank(tank, 0);
                }
                tk_debug_internal(DEBUG_ENEMY_MUGGLE_TANK, "%s向前移动(angle_deg:%f, pos:(%f,%f))\n", tank->name, tank->angle_deg, POS(tank->position));
                tank->key_value_for_control.mask = 0;
                SET_FLAG(&(tank->key_value_for_control), mask, TK_KEY_W_ACTIVE); // 默认向前移动
                handle_key(tank, &(tank->key_value_for_control));
                // if ((tk_shared_game_state.game_time % 20) == 0) {
                //     create_shell_for_tank(tank);
                // }
    iter_next_tank:
                continue;
            }
        }
    }
}

/**
 * 检查两个矩形是否在x轴上的投影区间有重叠
 */
static bool is_x_projection_overlap(const Rectangle* r1, const Rectangle* r2) {
    // 获取r1在x轴上的最小和最大值
    tk_float32_t r1_min_x = r1->lefttop.x;
    tk_float32_t r1_max_x = r1->lefttop.x;
    r1_min_x = MIN(r1->righttop.x, r1_min_x);
    r1_min_x = MIN(r1->rightbottom.x, r1_min_x);
    r1_min_x = MIN(r1->leftbottom.x, r1_min_x);
    r1_max_x = MAX(r1->righttop.x, r1_max_x);
    r1_max_x = MAX(r1->rightbottom.x, r1_max_x);
    r1_max_x = MAX(r1->leftbottom.x, r1_max_x);

    // 获取r2在x轴上的最小和最大值
    tk_float32_t r2_min_x = r2->lefttop.x;
    tk_float32_t r2_max_x = r2->lefttop.x;
    r2_min_x = MIN(r2->righttop.x, r2_min_x);
    r2_min_x = MIN(r2->rightbottom.x, r2_min_x);
    r2_min_x = MIN(r2->leftbottom.x, r2_min_x);
    r2_max_x = MAX(r2->righttop.x, r2_max_x);
    r2_max_x = MAX(r2->rightbottom.x, r2_max_x);
    r2_max_x = MAX(r2->leftbottom.x, r2_max_x);

    // 检查x轴投影是否重叠：r1在r2左侧或右侧时不重叠
    return !((r1_max_x < r2_min_x) || (r2_max_x < r1_min_x));
}

/**
 * 检查两个矩形是否在y轴上的投影区间有重叠
 */
static bool is_y_projection_overlap(const Rectangle* r1, const Rectangle* r2) {
    // 获取r1在y轴上的最小和最大值
    tk_float32_t r1_min_y = r1->lefttop.y;
    tk_float32_t r1_max_y = r1->lefttop.y;
    r1_min_y = MIN(r1->righttop.y, r1_min_y);
    r1_min_y = MIN(r1->rightbottom.y, r1_min_y);
    r1_min_y = MIN(r1->leftbottom.y, r1_min_y);
    r1_max_y = MAX(r1->righttop.y, r1_max_y);
    r1_max_y = MAX(r1->rightbottom.y, r1_max_y);
    r1_max_y = MAX(r1->leftbottom.y, r1_max_y);

    // 获取r2在y轴上的最小和最大值
    tk_float32_t r2_min_y = r2->lefttop.y;
    tk_float32_t r2_max_y = r2->lefttop.y;
    r2_min_y = MIN(r2->righttop.y, r2_min_y);
    r2_min_y = MIN(r2->rightbottom.y, r2_min_y);
    r2_min_y = MIN(r2->leftbottom.y, r2_min_y);
    r2_max_y = MAX(r2->righttop.y, r2_max_y);
    r2_max_y = MAX(r2->rightbottom.y, r2_max_y);
    r2_max_y = MAX(r2->leftbottom.y, r2_max_y);

    // 检查y轴投影是否重叠：r1在r2上方或下方时不重叠
    return !((r1_max_y < r2_min_y) || (r2_max_y < r1_min_y));
}

/**
 * 使用投影法判断两个矩形是否相交
 */
bool is_rectangle_collision_projection(const Rectangle* r1, const Rectangle* r2) {
    // 当且仅当x轴和y轴投影都重叠时，矩形相交
    return is_x_projection_overlap(r1, r2) && is_y_projection_overlap(r1, r2);
}

/**
 * 计算两点之间的向量
 */
Vector2 vector_from_points(const Point* a, const Point* b) {
    Vector2 v = {b->x - a->x, b->y - a->y};
    return v;
}

/**
 * 计算向量的点积
 */
tk_float32_t dot_product(const Vector2* a, const Vector2* b) {
    return a->x * b->x + a->y * b->y;
}

/**
 * 计算向量的法线（垂直向量）
 */
Vector2 normal_vector(const Vector2* v) {
    Vector2 n = {-v->y, v->x};
    return n;
}

/**
 * 归一化向量
 */
Vector2 normalize(const Vector2* v) {
    tk_float32_t length = sqrtf(v->x * v->x + v->y * v->y);
    if (length < 0.0001f) { // 避免除以零
        return (Vector2){0, 0};
    }
    return (Vector2){v->x / length, v->y / length};
}

/**
 * 将多边形投影到轴上，返回投影区间的最小值和最大值
 */
void project_polygon(const Vector2* axis, const Rectangle* rect, tk_float32_t* min, tk_float32_t* max) {
    // 计算矩形四个顶点在轴上的投影
    tk_float32_t p1 = dot_product(axis, &rect->lefttop);
    tk_float32_t p2 = dot_product(axis, &rect->righttop);
    tk_float32_t p3 = dot_product(axis, &rect->rightbottom);
    tk_float32_t p4 = dot_product(axis, &rect->leftbottom);
    
    // 找出投影区间的最小值和最大值
    *min = p1;
    *max = p1;
    if (p2 < *min) *min = p2;
    if (p2 > *max) *max = p2;
    if (p3 < *min) *min = p3;
    if (p3 > *max) *max = p3;
    if (p4 < *min) *min = p4;
    if (p4 > *max) *max = p4;
}

/**
 * 检查两个投影区间是否重叠
 */
bool overlap_projections(tk_float32_t min1, tk_float32_t max1, tk_float32_t min2, tk_float32_t max2) {
    return !(max1 < min2 || max2 < min1);
}

/**
 * 计算两个投影区间的重叠量
 */
tk_float32_t overlap_amount(tk_float32_t min1, tk_float32_t max1, tk_float32_t min2, tk_float32_t max2) {
    if (!overlap_projections(min1, max1, min2, max2)) {
        return 0;
    }
    return fminf(max1, max2) - fmaxf(min1, min2);
}

/**
 * 使用分离轴定理检测两个矩形是否碰撞
 */
bool is_rectangle_collision(const Rectangle* r1, const Rectangle* r2) {
    // 定义矩形的四条边向量（实际只需要两个不平行的边）
    Vector2 edges[4];
    
    // r1的两条边
    edges[0] = vector_from_points(&r1->lefttop, &r1->righttop);
    edges[1] = vector_from_points(&r1->righttop, &r1->rightbottom);
    
    // r2的两条边
    edges[2] = vector_from_points(&r2->lefttop, &r2->righttop);
    edges[3] = vector_from_points(&r2->righttop, &r2->rightbottom);
    
    // 检查所有可能的分离轴（边的法线）
    for (int i = 0; i < 4; i++) {
        // 计算边的法线作为分离轴
        Vector2 normal = normal_vector(&edges[i]);
        Vector2 axis = normalize(&normal);
        
        // 将两个矩形投影到轴上
        tk_float32_t min1, max1, min2, max2;
        project_polygon(&axis, r1, &min1, &max1);
        project_polygon(&axis, r2, &min2, &max2);
        
        // 如果投影区间不重叠，则矩形不相交
        if (!overlap_projections(min1, max1, min2, max2)) {
            return false;
        }
    }
    
    // 如果所有轴上的投影都重叠，则矩形相交
    return true;
}

bool is_two_tanks_collision(Tank *my_tank, Rectangle *newest_outline, Tank *other_tank) {
    Grid my_grid = get_grid_by_tank_position(&my_tank->position);
    Grid other_grid = get_grid_by_tank_position(&other_tank->position);

    if ((abs(my_grid.x - other_grid.x) >= 2) || (abs(my_grid.y - other_grid.y) >= 2)) {
        return false;
    }
    if (!is_rectangle_collision_projection(newest_outline, &other_tank->practical_outline)) {
        return false;
    }
    return is_rectangle_collision(newest_outline, &other_tank->practical_outline);
}

/*坦克是否与其他坦克发生碰撞*/
bool is_my_tank_collide_with_other_tanks(Tank *my_tank, Rectangle *newest_outline) {
    Tank *other_tank= NULL;
    TAILQ_FOREACH(other_tank, &tk_shared_game_state.tank_list, chain) {
        if (other_tank == my_tank) {
            continue;
        }
        if ((other_tank->health <= 0) || !TST_FLAG(other_tank, flags, TANK_ALIVE)) {
            continue;
        }
        if (is_two_tanks_collision(my_tank, newest_outline, other_tank)) {
            tk_debug_internal(DEBUG_TANK_COLLISION, "坦克(%s)检测到与坦克(%s)发生了碰撞！\n", my_tank->name, other_tank->name);
            return true;
        }
    }
    return false;
}

void calculate_shell_outline(const Point *center, Rectangle *rect) {
	Point *points = (Point *)rect;
    // 四个顶点坐标
    points[0] = (Point){center->x - (SHELL_RADIUS_LENGTH/2), center->y - (SHELL_RADIUS_LENGTH/2)};
    points[1] = (Point){center->x + (SHELL_RADIUS_LENGTH/2), center->y - (SHELL_RADIUS_LENGTH/2)};
    points[2] = (Point){center->x + (SHELL_RADIUS_LENGTH/2), center->y + (SHELL_RADIUS_LENGTH/2)};
    points[3] = (Point){center->x - (SHELL_RADIUS_LENGTH/2), center->y + (SHELL_RADIUS_LENGTH/2)};
}

bool is_shell_and_tank_collision(Shell *shell, Rectangle *shell_outline, Tank *tank) {
    Grid shell_grid = get_grid_by_tank_position(&shell->position);
    Grid tank_grid = get_grid_by_tank_position(&tank->position);

    if ((abs(shell_grid.x - tank_grid.x) >= 2) || (abs(shell_grid.y - tank_grid.y) >= 2)) {
        return false;
    }
    if (!is_rectangle_collision_projection(shell_outline, &tank->practical_outline)) {
        return false;
    }
    return is_rectangle_collision(shell_outline, &tank->practical_outline);
}

/*炮弹是否与其他坦克发生碰撞，函数返回发生碰撞的其他人坦克*/
Tank* is_my_shell_collide_with_other_tanks(Shell *shell) {
    Tank *other_tank= NULL;
    Tank *my_tank = (Tank *)(shell->tank_owner);
    Rectangle shell_outline;

    calculate_shell_outline(&shell->position, &shell_outline);
    TAILQ_FOREACH(other_tank, &tk_shared_game_state.tank_list, chain) {
        if (other_tank == my_tank) { //如果自己的炮弹打到自己，不掉血，直接穿过？合理吗这样设定~
            continue;
        }
        if ((other_tank->health <= 0) || !TST_FLAG(other_tank, flags, TANK_ALIVE)) {
            continue;
        }
        if (is_shell_and_tank_collision(shell, &shell_outline, other_tank)) {
            tk_debug_internal(DEBUG_SHELL_COLLISION, "炮弹(%s's %u shell)检测到与坦克(%s)在位置(%f,%f)发生了碰撞！\n", 
                my_tank->name, shell->id, other_tank->name, POS(shell->position));
            return other_tank;
        }
    }
    return NULL;
}