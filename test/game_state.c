#include <stdlib.h>
#include <stdio.h>
#include "game_state.h"
#include <bsd/string.h>
#include <math.h>
#include "tools.h"

IDPool* tk_idpool = NULL;
GameState tk_shared_game_state;

Point tk_maze_offset = {20,20}; // 默认生成的地图左上角为(0,0)，导致地图位于窗口最左上角不太美观，整体将地图往右下移动一段偏移距离

extern Grid get_grid_by_tank_position(Point *pos);

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
    tank->position = pos;
    tank->angle_deg = angle_deg;
    tank->role = role;
    SET_FLAG(tank, flags, TANK_ALIVE);
    // tank->basic_color = (void *)((TANK_ROLE_SELF == tank->role) ? ID2COLORPTR(TK_BLUE) : ID2COLORPTR(TK_RED));
    tank->health = tank->max_health = (TANK_ROLE_SELF == tank->role) ? 500 : 250;
    tank->speed = TANK_INIT_SPEED;
    tank->max_shell_num = DEFAULT_TANK_SHELLS_MAX_NUM;
    calculate_tank_outline(&tank->position, TANK_LENGTH, TANK_WIDTH+4, calc_corrected_angle_deg(tank->angle_deg), &tank->practical_outline); // see handle_key()
    TAILQ_INIT(&tank->shell_list);

    if (TANK_ROLE_SELF == tank->role) {
        tk_shared_game_state.my_tank = tank;
    }
    TAILQ_INSERT_HEAD(&tk_shared_game_state.tank_list, tank, chain);

    tk_debug("create a tank(name:%s, id:%lu, total size:%luB, ExplodeEffect's size: %luB) success, total tank num %u\n", 
        tank->name, tank->id, sizeof(Tank), sizeof(tank->explode_effect), tank_num+1);
    return tank;
error:
    tk_debug("Error: create tank %s failed\n", name);
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
            TAILQ_REMOVE(&tk_shared_game_state.tank_list, t1, chain);
        }
    }
    TAILQ_FOREACH_SAFE(shell, &tank->shell_list, chain, tmp) {
        TAILQ_REMOVE(&tank->shell_list, shell, chain);
        delete_shell(shell, 0);
        shell_num++;
    }
    tk_debug("tank(id:%lu) %s(flags:%lu, score:%u, health:%u) is deleted, and free %u shells\n", 
        (tank)->id, (tank)->name, (tank)->flags, (tank)->score, (tank)->health, shell_num);
    id_pool_release(tk_idpool, tank->id);
    tank->id = 0;
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
}

void cleanup_game_state() {
    Tank *tank = NULL;
    Tank *tmp = NULL;
    tk_uint8_t tank_num = 0;

    TAILQ_FOREACH_SAFE(tank, &tk_shared_game_state.tank_list, chain, tmp) {
        TAILQ_REMOVE(&tk_shared_game_state.tank_list, tank, chain);
        delete_tank(tank, 0);
        tank_num++;
    }
    if (tk_shared_game_state.blocks) {
        free(tk_shared_game_state.blocks);
    }
    tk_shared_game_state.blocks_num = 0;
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
    tk_debug_internal(DEBUG_TEST, "get_ray_intersection_dot_with_grid: start(%f,%f), angle_deg(%f), grid(%d,%d)\n", 
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
        tk_debug_internal(DEBUG_TEST, "result(0): intersection(%f,%f), next_grid(%d,%d), k(%f), reflect_angle_deg(%f)\n", 
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
        tk_debug_internal(DEBUG_TEST, "result(1): intersection(%f,%f), next_grid(%d,%d), k(%f), reflect_angle_deg(%f)\n", 
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
        tk_debug_internal(DEBUG_TEST, "result(2): intersection(%f,%f), next_grid(%d,%d), k(%f), reflect_angle_deg(%f)\n", 
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
        tk_debug_internal(DEBUG_TEST, "result(3): intersection(%f,%f), next_grid(%d,%d), k(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(4): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(5): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(1-0): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(6): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(7): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(1-1): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(8): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(9): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(1-2): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(10): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(11): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                tk_debug_internal(DEBUG_TEST, "result(1-3): intersection(%f,%f), next_grid(%d,%d), k(%f), k0(%f), reflect_angle_deg(%f)\n", 
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
                    tk_debug_internal(DEBUG_TEST, ">1 | dot0(%f,%f), pos1(%f,%f), pos2(%f,%f), %d, %d, grid1:(%d,%d), grid2:(%d,%d), grid3:(%d,%d), grid4:(%d,%d)\n", 
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
                    tk_debug_internal(DEBUG_TEST, ">2 | dot0(%f,%f), pos1(%f,%f), pos2(%f,%f), %d, %d, grid1:(%d,%d), grid2:(%d,%d), grid3:(%d,%d), grid4:(%d,%d)\n", 
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
                    tk_debug_internal(DEBUG_TEST, ">3 | dot0(%f,%f), pos1(%f,%f), pos2(%f,%f), %d, %d, grid1:(%d,%d), grid2:(%d,%d), grid3:(%d,%d), grid4:(%d,%d)\n", 
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
                    tk_debug_internal(DEBUG_TEST, ">4 | dot0(%f,%f), pos1(%f,%f), pos2(%f,%f), %d, %d, grid1:(%d,%d), grid2:(%d,%d), grid3:(%d,%d), grid4:(%d,%d)\n", 
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
    // tk_debug_internal(DEBUG_TEST, "grid:(%d, %d)/%d, (%d, %d)/%d, (%d, %d)/%d, (%d, %d)/%d\n", 
    //     POS(grid0), grid_id(&grid0), POS(grid1), grid_id(&grid1), POS(grid2), grid_id(&grid2), POS(grid3), grid_id(&grid3));

    tank->collision_flag = 0;
    if (is_two_pos_transfer_through_wall(&outline.righttop, &outline.rightbottom)) {
        tk_debug_internal(DEBUG_TEST, "前方发生碰撞\n");
        SET_FLAG(tank, collision_flag, COLLISION_FRONT);
    } else if (is_two_pos_transfer_through_wall(&outline.lefttop, &outline.righttop)) {
        tk_debug_internal(DEBUG_TEST, "左侧发生碰撞\n");
        SET_FLAG(tank, collision_flag, COLLISION_LEFT);
    } else if (is_two_pos_transfer_through_wall(&outline.rightbottom, &outline.leftbottom)) {
        tk_debug_internal(DEBUG_TEST, "右侧发生碰撞\n");
        SET_FLAG(tank, collision_flag, COLLISION_RIGHT);
    } else if (is_two_pos_transfer_through_wall(&outline.leftbottom, &outline.lefttop)) {
        tk_debug_internal(DEBUG_TEST, "后方发生碰撞\n");
        SET_FLAG(tank, collision_flag, COLLISION_BACK);
    } else { // 未发生碰撞
        tank->position = new_position;
        tank->angle_deg = new_angle_deg;
        tank->practical_outline = outline;
    }
    tank->outline = outline; // 将可能发生了碰撞的最新轮廓绘制出来用于debug
}

Shell* create_shell_for_tank(Tank *tank) {
    if (!tank) return NULL;
    if (!TST_FLAG(tank, flags, TANK_ALIVE)) return NULL;

    Shell *shell = NULL;
    tk_uint8_t shell_num = 0;
    TAILQ_FOREACH(shell, &tank->shell_list, chain) {
        shell_num++;
    }
    if (shell_num >= tank->max_shell_num) {
        tk_debug("Warn: can't create more shells(%u>=MAX/%u) for tank(%s)\n", shell_num, tank->max_shell_num, tank->name);
        return NULL;
    }

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
    TAILQ_INSERT_HEAD(&tank->shell_list, shell, chain);
    tk_debug("create a shell(id:%lu) at (%f,%f) for tank(%s) success, the tank now has %u shells\n", shell->id, 
        POS(shell->position), tank->name, shell_num+1);
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
    tk_debug("shell(id:%lu) of tank(%s) is deleted\n", shell->id, ((Tank*)(shell->tank_owner))->name);
    if (dereference) {
        TAILQ_FOREACH_SAFE(s, &((Tank*)(shell->tank_owner))->shell_list, chain, t) {
            if (s != shell) {
                continue;
            }
            TAILQ_REMOVE(&((Tank*)(shell->tank_owner))->shell_list, s, chain);
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

#define FINETUNE_SHELL_RADIUS_LENGTH (SHELL_RADIUS_LENGTH+0)

void update_one_shell_movement_position(Shell *shell) {
    Point new_pos;
    Grid current_grid;
    tk_float32_t wall_x = 0;
    tk_float32_t wall_y = 0;
    Point p;
    Grid next_grid;
    tk_float32_t new_angle_deg = 0;
    tk_uint8_t collide_wall_x = 0; // 水平墙壁
    tk_uint8_t collide_wall_y = 0; // 垂直墙壁

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
                goto out;
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
                goto out;
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
                goto out;
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
                goto out;
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
            tk_debug_internal(1, "current_grid(%d,%d), wall_x(%f, wall_y(%f)), collide_wall(%d,%d)\n", current_grid.x, current_grid.y, 
                wall_x, wall_y, collide_wall_x, collide_wall_y);
            if (collide_wall_x && collide_wall_y) { // 碰撞墙角
                tk_debug_internal(1, "触碰墙角(假设上) | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x + ((shell->position.y - (new_pos.y)) / calculate_tan(90 - shell->angle_deg));
                new_angle_deg = 180 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) > wall_y) {
                    tk_debug_internal(1, "触碰墙角(实际右) | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                        shell->angle_deg, wall_y, POS(new_pos));
                    new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.y = shell->position.y - ((new_pos.x - shell->position.x) / calculate_tan(shell->angle_deg));
                    new_angle_deg = 360 - shell->angle_deg;
                    tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                } else if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) == wall_y) { // 刚好触碰两面墙壁，原路反弹回去
                    tk_debug_internal(1, "触碰墙角(刚好触碰右上两面墙壁)\n");
                    new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_angle_deg = shell->angle_deg + 180;
                    if (new_angle_deg >= 360) {
                        new_angle_deg -= 360;
                    }
                }
            } else if (collide_wall_x && !collide_wall_y) { // 碰撞(上)单面墙壁
                tk_debug_internal(1, "触碰(上)单面墙壁 | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x + ((shell->position.y - (new_pos.y)) / calculate_tan(90 - shell->angle_deg));
                new_angle_deg = 180 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else if (!collide_wall_x && collide_wall_y) { // 碰撞(右)单面墙壁
                tk_debug_internal(1, "触碰(右)单面墙壁 | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_y, POS(new_pos));
                new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.y = shell->position.y - ((new_pos.x - shell->position.x) / calculate_tan(shell->angle_deg));
                new_angle_deg = 360 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
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
            tk_debug_internal(1, "current_grid(%d,%d), wall_x(%f, wall_y(%f)), collide_wall(%d,%d)\n", current_grid.x, current_grid.y, 
                wall_x, wall_y, collide_wall_x, collide_wall_y);
            if (collide_wall_x && collide_wall_y) {
                tk_debug_internal(1, "触碰墙角(假设下) | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x + ((new_pos.y - shell->position.y) / calculate_tan(shell->angle_deg - 90));
                new_angle_deg = 180 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) > wall_y) {
                    tk_debug_internal(1, "触碰墙角(实际右) | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                        shell->angle_deg, wall_y, POS(new_pos));
                    new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.y = shell->position.y + ((new_pos.x - shell->position.x) / calculate_tan(180 - shell->angle_deg));
                    new_angle_deg = 360 - shell->angle_deg;
                    tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                } else if ((new_pos.x+FINETUNE_SHELL_RADIUS_LENGTH) == wall_y) {
                    tk_debug_internal(1, "触碰墙角(刚好触碰右下两面墙壁)\n");
                    new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_angle_deg = shell->angle_deg + 180;
                    if (new_angle_deg >= 360) {
                        new_angle_deg -= 360;
                    }
                }
            } else if (collide_wall_x && !collide_wall_y) { // 碰撞(下)单面墙壁
                tk_debug_internal(1, "触碰(下)单面墙壁 | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x + ((new_pos.y - shell->position.y) / calculate_tan(shell->angle_deg - 90));
                new_angle_deg = 180 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else if (!collide_wall_x && collide_wall_y) { // 碰撞(右)单面墙壁
                tk_debug_internal(1, "触碰(右)单面墙壁 | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_y, POS(new_pos));
                new_pos.x = wall_y - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.y = shell->position.y + ((new_pos.x - shell->position.x) / calculate_tan(180 - shell->angle_deg));
                new_angle_deg = 360 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
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
            tk_debug_internal(1, "current_grid(%d,%d), wall_x(%f, wall_y(%f)), collide_wall(%d,%d)\n", current_grid.x, current_grid.y, 
                wall_x, wall_y, collide_wall_x, collide_wall_y);
            if (collide_wall_x && collide_wall_y) {
                tk_debug_internal(1, "触碰墙角(假设下) | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x - ((new_pos.y - shell->position.y) / calculate_tan(270 - shell->angle_deg));
                new_angle_deg = 540 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) < wall_y) {
                    tk_debug_internal(1, "触碰墙角(实际左) | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                        shell->angle_deg, wall_y, POS(new_pos));
                    new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.y = shell->position.y + ((shell->position.x - new_pos.x) / calculate_tan(shell->angle_deg - 180));
                    new_angle_deg = 360 - shell->angle_deg;
                    tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                } else if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) == wall_y) {
                    tk_debug_internal(1, "触碰墙角(刚好触碰左下两面墙壁)\n");
                    new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_angle_deg = shell->angle_deg + 180;
                    if (new_angle_deg >= 360) {
                        new_angle_deg -= 360;
                    }
                }
            } else if (collide_wall_x && !collide_wall_y) { // 碰撞(下)单面墙壁
                tk_debug_internal(1, "触碰(下)单面墙壁 | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x - FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x - ((new_pos.y - shell->position.y) / calculate_tan(270 - shell->angle_deg));
                new_angle_deg = 540 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else if (!collide_wall_x && collide_wall_y) { // 碰撞(左)单面墙壁
                tk_debug_internal(1, "触碰(左)单面墙壁 | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_y, POS(new_pos));
                new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.y = shell->position.y + ((shell->position.x - new_pos.x) / calculate_tan(shell->angle_deg - 180));
                new_angle_deg = 360 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
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
            tk_debug_internal(1, "current_grid(%d,%d), wall_x(%f, wall_y(%f)), collide_wall(%d,%d)\n", current_grid.x, current_grid.y, 
                wall_x, wall_y, collide_wall_x, collide_wall_y);
            if (collide_wall_x && collide_wall_y) {
                tk_debug_internal(1, "触碰墙角(假设上) | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x - ((shell->position.y - (new_pos.y)) / calculate_tan(shell->angle_deg - 270));
                new_angle_deg = 540 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) < wall_y) {
                    tk_debug_internal(1, "触碰墙角(实际左) | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                        shell->angle_deg, wall_y, POS(new_pos));
                    new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.y = shell->position.y - ((shell->position.x - new_pos.x) / calculate_tan(360 - shell->angle_deg));
                    new_angle_deg = 360 - shell->angle_deg;
                    tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
                } else if ((new_pos.x-FINETUNE_SHELL_RADIUS_LENGTH) == wall_y) {
                    tk_debug_internal(1, "触碰墙角(刚好触碰左上两面墙壁)\n");
                    new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                    new_angle_deg = shell->angle_deg + 180;
                    if (new_angle_deg >= 360) {
                        new_angle_deg -= 360;
                    }
                }
            } else if (collide_wall_x && !collide_wall_y) { // 碰撞(上)单面墙壁
                tk_debug_internal(1, "触碰(上)单面墙壁 | pos(%f,%f), angle_deg(%f), wallx(%f), new_pos(%f,%f)\n", POS(shell->position), 
                    shell->angle_deg, wall_x, POS(new_pos));
                new_pos.y = wall_x + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.x = shell->position.x - ((shell->position.y - (new_pos.y)) / calculate_tan(shell->angle_deg - 270));
                new_angle_deg = 540 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else if (!collide_wall_x && collide_wall_y) { // 碰撞(左)单面墙壁
                tk_debug_internal(1, "触碰(左)单面墙壁 | pos(%f,%f), angle_deg(%f), wally(%f), new_pos(%f,%f)\n", POS(shell->position), 
                        shell->angle_deg, wall_y, POS(new_pos));
                new_pos.x = wall_y + FINETUNE_SHELL_RADIUS_LENGTH;
                new_pos.y = shell->position.y - ((shell->position.x - new_pos.x) / calculate_tan(360 - shell->angle_deg));
                new_angle_deg = 360 - shell->angle_deg;
                tk_debug_internal(1, "fixed_new_pos(%f,%f), new_angle_deg(%f)\n", POS(new_pos), new_angle_deg);
            } else {
                goto out;
            }
        }
    }

out: // 没有发生碰撞反弹直接out
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
            // tk_debug_internal(1, "update shell %lu, pos(%f,%f)\n", shell->id, POS(shell->position));
            old_pos = shell->position;
            update_one_shell_movement_position(shell);
            tk_debug_internal(1, "move from (%f,%f) to (%f,%f)\n", POS(old_pos), POS(shell->position));
            /*如果上次移动位置即将触碰墙壁，本次前进则会检测到碰撞，因此本次前进的步伐非常之微小，可以认为前后都处于同一位置，
            简单来说，正常一个位置只有一帧画面的话，那现在就变成两帧都在同一位置，会使得玩家观察到碰撞反弹处炮弹迟滞一段时间的现象，
            对于这种情况，需要再次执行前进动作*/
            if (is_near(POS(old_pos), POS(shell->position))) {
                update_one_shell_movement_position(shell);
                tk_debug_internal(1, "本次移动距离太小，再次移动！(%f,%f)=>(%f,%f)\n", POS(old_pos), POS(shell->position));
            }
        }
    }
}