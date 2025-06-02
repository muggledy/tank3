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
        tk_debug("Error: id_pool_allocate failed\n");
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
    TAILQ_INIT(&tank->shell_list);

    TAILQ_INSERT_HEAD(&tk_shared_game_state.tank_list, tank, chain);
    if (TANK_ROLE_SELF == tank->role) {
        tk_shared_game_state.my_tank = tank;
    }

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

void delete_tank(Tank *tank) {
    Shell *shell = NULL;
    Shell *tmp = NULL;
    tk_uint8_t shell_num = 0;

    if (!tank) {
        return;
    }
    if (TANK_ROLE_SELF == tank->role) {
        tk_shared_game_state.my_tank = NULL;
    }
    TAILQ_FOREACH_SAFE(shell, &tank->shell_list, chain, tmp) {
        TAILQ_REMOVE(&tank->shell_list, shell, chain);
        free(shell);
        shell_num++;
    }
    tk_debug("tank(id:%lu) %s(flags:%lu, score:%u, health:%u) is deleted, and free %u shells\n", 
        (tank)->id, (tank)->name, (tank)->flags, (tank)->score, (tank)->health, shell_num);
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
        delete_tank(tank);
        tank_num++;
    }
    if (tk_shared_game_state.blocks) {
        free(tk_shared_game_state.blocks);
    }
    tk_debug("total %u tanks are all freed\n", tank_num);
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
    end.x = start.x + distance * cosf(direction_rad);
    end.y = start.y - distance * sinf(direction_rad);
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
static void calculate_tank_outline(const Point *center, tk_float32_t width, tk_float32_t height, tk_float32_t angle_deg, Rectangle *rect) {
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
                    return 1;
                }
                return 0;
            } else if (t == DOT_RIGHT_OF_LINE) { // dot0在线pos1-pos2的下方
                if (!is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid3) 
                    || !is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid3)) {
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
            }
            grid3 = (Grid){grid2.x, grid1.y};
            grid4 = (Grid){grid1.x, grid2.y};
            dot0 = get_pos_by_grid(&grid3, 3);
            t = get_point_position_with_line(&dot0, pos1, pos2);
            if (t == DOT_LEFT_OF_LINE) { // dot0在线pos1-pos2的下方
                if (!is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid3) 
                    || !is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid3)) {
                    return 1;
                }
                return 0;
            } else if (t == DOT_RIGHT_OF_LINE) { // dot0在线pos1-pos2的上方
                if (!is_two_grids_connected(&tk_shared_game_state.maze, &grid1, &grid4) 
                    || !is_two_grids_connected(&tk_shared_game_state.maze, &grid2, &grid4)) {
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

    // 计算坦克轮廓矩形四个角所处的网格
    Grid grid0 = get_grid_by_tank_position(&outline.righttop);
    Grid grid1 = get_grid_by_tank_position(&outline.rightbottom);
    Grid grid2 = get_grid_by_tank_position(&outline.leftbottom);
    Grid grid3 = get_grid_by_tank_position(&outline.lefttop);
    tk_debug_internal(DEBUG_TEST, "grid:(%d, %d)/%d, (%d, %d)/%d, (%d, %d)/%d, (%d, %d)/%d\n", 
        POS(grid0), grid_id(&grid0), POS(grid1), grid_id(&grid1), POS(grid2), grid_id(&grid2), POS(grid3), grid_id(&grid3));

    if (is_two_pos_transfer_through_wall(&outline.righttop, &outline.rightbottom)) {
        tk_debug_internal(1, "前方发生碰撞\n");
    } else if (is_two_pos_transfer_through_wall(&outline.lefttop, &outline.righttop)) {
        tk_debug_internal(1, "左侧发生碰撞\n");
    } else if (is_two_pos_transfer_through_wall(&outline.rightbottom, &outline.leftbottom)) {
        tk_debug_internal(1, "右侧发生碰撞\n");
    } else if (is_two_pos_transfer_through_wall(&outline.leftbottom, &outline.lefttop)) {
        tk_debug_internal(1, "后方发生碰撞\n");
    } else { // 未发生碰撞
        tank->position = new_position;
        tank->angle_deg = new_angle_deg;
        // tank->outline = outline;
    }
    tank->outline = outline;
}