#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "maze.h"
#include "debug.h"

// 方向数组：上、左、下、右
static const int dx[] = {0, -1, 0, 1};
static const int dy[] = {-1, 0, 1, 0};

// 获取网格ID
int grid_id(Grid *g) {
    return g->y * HORIZON_GRID_NUMBER + g->x;
}

// 判断网格是否合法，合法返回1
int is_grid_valid(Grid *g) {
    if ((g->x < 0) || (g->y < 0) || (g->x >= HORIZON_GRID_NUMBER) || (g->y >= VERTICAL_GRID_NUMBER)) {
        return 0;
    }
    return 1;
}

// 判断两个网格是否上下或左右相邻（调用者自己应确保输入的网格参数合法：is_grid_valid return 1）
int is_two_grids_adjacent(Grid *g1, Grid *g2) {
    if (!((g1->x == g2->x) || (g1->y == g2->y))) { // 非相邻网格
        if ((abs((g1->y - g2->y)) == 1) && (abs((g1->x - g2->x)) == 1)) {
            return MAZE_DIAGONAL_GRID; // 对角相连的两个网格
        }
        return MAZE_UNRELATED_GRID;
    }
    if ((g1->x == g2->x) && (g1->y == g2->y)) {
        return MAZE_SAME_GRID; // 同一个网格
    }
    if (g1->x == g2->x) {
        if (abs((g1->y - g2->y)) == 1) {
            return MAZE_ADJACENT_GRID; // 相邻网格
        }
    }
    if (g1->y == g2->y) {
        if (abs((g1->x - g2->x)) == 1) {
            return MAZE_ADJACENT_GRID;
        }
    }
    return MAZE_UNRELATED_GRID; // can't be here
}

// 判断两个网格之间是否打通（调用者自己应确保输入的两个网格是上下左右相邻的：is_two_grids_adjacent return MAZE_ADJACENT_GRID。
// 因为对于非上下左右相邻的网格，没有“打通”这一说）
int is_two_grids_connected(Maze* maze, Grid *g1, Grid *g2) {
    return maze->map[grid_id(g1)][grid_id(g2)];
}

int is_two_grids_the_same(Grid *g1, Grid *g2) {
    return ((g1->x == g2->x) && (g1->y == g2->y));
}

// 生成随机数
int get_random_number(int min, int max) {
    return rand() % (max - min + 1) + min;
}

// 初始化迷宫
void maze_init(Maze* maze) {
    memset(maze->map, 0, sizeof(maze->map));
    memset(maze->vis, 0, sizeof(maze->vis));
}

// 生成迷宫（Prim遍历墙算法）
void maze_generate(Maze* maze) {
    srand(time(NULL));
    maze_init(maze);
    
    Wall* walls = malloc(sizeof(Wall) * HORIZON_GRID_NUMBER * VERTICAL_GRID_NUMBER * 4); // 候选墙列表
    int wall_count = 0; // 候选墙数量
    
    // 初始网格
    maze->vis[0][0] = 1; // vis数组记录的是任意网格是否已被访问过，譬如vis[i][j]=1表示第i列第j行的网格已访问过
    walls[wall_count++] = (Wall){ {0, 0}, {1, 0} };
    
    while (wall_count > 0) {
        // 从候选墙列表中随机选择一堵墙来打通
        int n = get_random_number(0, wall_count - 1);
        Wall wall = walls[n];

        // 并从候选墙列表中移除所选的这堵墙
        walls[n] = walls[wall_count - 1];
        wall_count--;
        
        if (!maze->vis[wall.second.x][wall.second.y]) { // 如果墙壁对应的网格已访问过，说明这个网格已经被打通了，那就不能再从另一个方向打通它，也就是不能进入if块
            // 打通两个网格
            int id1 = grid_id(&wall.first); // 第i列第j行网格的id为：i + HORIZON_GRID_NUMBER*j，譬如5行3列的地图，总计15个网格，第1行1列的网格就是第0个网格，最后一行最后一列的网格就是第14个网格
            int id2 = grid_id(&wall.second);
            maze->map[id1][id2] = maze->map[id2][id1] = 1; // map数组标识的含义是：任意相邻两个网格之间是否打通，譬如map[m][n]=1表示第m个网格和第n个网格之间已被打通，但注意只能记录和查询上下左右相邻网格之间的打通状态
            
            maze->vis[wall.second.x][wall.second.y] = 1;
            
            // 添加相邻的墙到候选墙列表，以继续探访未知区域，直到所有网格都已被访问过，到时候就没有候选墙了
            for (int i = 0; i < 4; i++) {
                int nx = wall.second.x + dx[i];
                int ny = wall.second.y + dy[i];
                
                if (nx < 0 || nx >= HORIZON_GRID_NUMBER || 
                    ny < 0 || ny >= VERTICAL_GRID_NUMBER) {
                    continue;
                }
                
                if (maze->vis[nx][ny]) continue; // 如果墙壁对应的网格已访问过，则该墙壁不能再放入候选列表
                
                Grid next = {nx, ny};
                int current_id = grid_id(&wall.second);
                int next_id = grid_id(&next);
                
                if (maze->map[current_id][next_id] == 0) {
                    walls[wall_count++] = (Wall){wall.second, next};
                }
            }
        }
    }
    
    free(walls);
}

Block* get_block_positions(Maze* maze, tk_uint16_t* block_count) {
    Block* blocks = malloc(sizeof(Block) * (HORIZON_GRID_NUMBER * VERTICAL_GRID_NUMBER * 2 + 4));
    *block_count = 0;
    
    // 检查垂直墙壁
    for (int y = 0; y < VERTICAL_GRID_NUMBER; y++) {
        for (int x = 0; x < HORIZON_GRID_NUMBER - 1; x++) {
            Grid g1 = {x, y};
            Grid g2 = {x + 1, y};
            if (maze->map[grid_id(&g1)][grid_id(&g2)] == 0) {
                blocks[*block_count].start = (Vec){(x + 1) * GRID_SIZE, y * GRID_SIZE};
                blocks[*block_count].end = (Vec){(x + 1) * GRID_SIZE, (y + 1) * GRID_SIZE};
                (*block_count)++;
            }
        }
    }
    
    // 检查水平墙壁
    for (int x = 0; x < HORIZON_GRID_NUMBER; x++) {
        for (int y = 0; y < VERTICAL_GRID_NUMBER - 1; y++) {
            Grid g1 = {x, y};
            Grid g2 = {x, y + 1};
            if (maze->map[grid_id(&g1)][grid_id(&g2)] == 0) {
                blocks[*block_count].start = (Vec){x * GRID_SIZE, (y + 1) * GRID_SIZE};
                blocks[*block_count].end = (Vec){(x + 1) * GRID_SIZE, (y + 1) * GRID_SIZE};
                (*block_count)++;
            }
        }
    }
    
    // 上下左右边界墙壁
    blocks[*block_count].start = (Vec){0, 0};
    blocks[*block_count].end = (Vec){GRID_SIZE*HORIZON_GRID_NUMBER, 0};
    (*block_count)++;
    blocks[*block_count].start = (Vec){0, GRID_SIZE*VERTICAL_GRID_NUMBER};
    blocks[*block_count].end = (Vec){GRID_SIZE*HORIZON_GRID_NUMBER, GRID_SIZE*VERTICAL_GRID_NUMBER};
    (*block_count)++;
    blocks[*block_count].start = (Vec){0, 0};
    blocks[*block_count].end = (Vec){0, GRID_SIZE*VERTICAL_GRID_NUMBER};
    (*block_count)++;
    blocks[*block_count].start = (Vec){GRID_SIZE*HORIZON_GRID_NUMBER, 0};
    blocks[*block_count].end = (Vec){GRID_SIZE*HORIZON_GRID_NUMBER, GRID_SIZE*VERTICAL_GRID_NUMBER};
    (*block_count)++;
    return blocks;
}

// 更直观的网格墙打印函数
void print_maze_walls(Maze* maze) {
    tk_debug("Maze Wall Visualization(%dx%d):\n", VERTICAL_GRID_NUMBER, HORIZON_GRID_NUMBER);
    
    // 打印顶部边界
    printf("+");
    for (int x = 0; x < HORIZON_GRID_NUMBER; x++) {
        printf("---+");
    }
    printf("\n");
    
    for (int y = 0; y < VERTICAL_GRID_NUMBER; y++) {
        // 打印垂直墙壁和房间
        printf("|");
        for (int x = 0; x < HORIZON_GRID_NUMBER; x++) {
            Grid current = {x, y};
            
            // 打印房间内容（这里可以自定义）
            printf("   ");
            
            // 打印右侧垂直墙
            if (x < HORIZON_GRID_NUMBER - 1) {
                Grid right = {x + 1, y};
                printf("%s", maze->map[grid_id(&current)][grid_id(&right)] ? " " : "|");
            } else {
                printf("|");
            }
        }
        printf("\n");
        
        // 打印水平墙壁
        if (y < VERTICAL_GRID_NUMBER - 1) {
            printf("+");
            for (int x = 0; x < HORIZON_GRID_NUMBER; x++) {
                Grid current = {x, y};
                Grid below = {x, y + 1};
                printf("%s", maze->map[grid_id(&current)][grid_id(&below)] ? "   " : "---");
                printf("+");
            }
            printf("\n");
        }
    }
    
    // 打印底部边界
    printf("+");
    for (int x = 0; x < HORIZON_GRID_NUMBER; x++) {
        printf("---+");
    }
    printf("\n");
}

void bfs_shortest_path_search(void *maze_path_bfs_search_manager) {
    MazePathBFSearchManager *manager = (MazePathBFSearchManager *)maze_path_bfs_search_manager;
    MazePathBFSearchNode *current = NULL;
    Grid next;
    tk_uint16_t i = 0;

    if (!manager->maze) {
        return;
    }
    if (!is_grid_valid(&manager->start) || !is_grid_valid(&manager->end)) {
        tk_debug("Error: %s's input param(start(%d,%d) or end(%d,%d)) is not valid\n", __func__, POS(manager->start), POS(manager->end));
        return;
    }
    memset(manager->maze_node_status_tbl, 0, sizeof(manager->maze_node_status_tbl));
    memset(manager->bfs_queue, 0, sizeof(manager->bfs_queue));
    manager->front = manager->rear = 0;
    manager->success = 0;

    manager->bfs_queue[0] = &(manager->maze_node_status_tbl[manager->start.x][manager->start.y]);
    manager->bfs_queue[0]->current = manager->start;
    manager->bfs_queue[0]->is_current_visited = 1;
    manager->rear++;

    while(manager->front < manager->rear) {
        current = manager->bfs_queue[manager->front++];
        if (is_two_grids_the_same(&current->current, &manager->end)) {
            manager->success = 1;
            if (!is_two_grids_the_same(&manager->start, &manager->end)) {
                tk_debug("找到BFS最短路径(%d,%d)->(%d,%d)：\n", POS(manager->start), POS(manager->end));
#if 0
                next = current->current;
                for (i = 0; i <= current->steps; i++) {
                    printf("(%d,%d)", POS(next));
                    if (0 != manager->maze_node_status_tbl[next.x][next.y].steps) {
                        printf("<-");
                        next = manager->maze_node_status_tbl[next.x][next.y].previous;
                    } else {
                        break;
                    }
                }
#else
                FOREACH_BFS_SEARCH_MANAGER_GRID(manager, next) {
                    printf("(%d,%d)", POS(next));
                    if (0 != manager->maze_node_status_tbl[next.x][next.y].steps) {
                        printf("<-");
                    }
                }
#endif
                printf("\n");
            }
            return;
        }
        // 探索四个方向
        for (i = 0; i < 4; i++) {
            next.x = current->current.x + dx[i];
            next.y = current->current.y + dy[i];
            if (is_grid_valid(&next) && !(manager->maze_node_status_tbl[next.x][next.y].is_current_visited) 
                    && is_two_grids_connected(manager->maze, &current->current, &next)) {
                manager->bfs_queue[manager->rear] = &(manager->maze_node_status_tbl[next.x][next.y]);
                manager->bfs_queue[manager->rear]->is_current_visited = 1;
                manager->bfs_queue[manager->rear]->current = next;
                manager->bfs_queue[manager->rear]->previous = current->current;
                manager->bfs_queue[manager->rear]->steps = current->steps + 1;
                // printf("=> (%d,%d), (%d,%d), steps %u\n", POS(manager->bfs_queue[manager->rear]->current), POS(manager->bfs_queue[manager->rear]->previous), 
                //     manager->bfs_queue[manager->rear]->steps);
                manager->rear++;
            }
        }
    }
}

#if 0
int main() {
    Maze maze;
    maze_generate(&maze);
    
    int block_count;
    Block* blocks = get_block_positions(&maze, &block_count);
    
    // 使用生成的墙壁...
    for (int i=0; i<block_count; i++) {
        printf("[(%f,%f),(%f,%f)], ", blocks[i].start.x, blocks[i].start.y, blocks[i].end.x, blocks[i].end.y);
    }
    
    // 打印更直观的墙壁表示
    print_maze_walls(&maze);
    
    free(blocks);
    return 0;
}
#endif