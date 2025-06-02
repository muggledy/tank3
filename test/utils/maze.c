#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 定义常量
#define HORIZON_GRID_NUMBER 4
#define VERTICAL_GRID_NUMBER 3
#define GRID_SIZE 40
#define MAX_GRID_ID (HORIZON_GRID_NUMBER * VERTICAL_GRID_NUMBER)

// 方向数组
static int dx[] = {0, -1, 0, 1};
static int dy[] = {-1, 0, 1, 0};

// 网格结构体
typedef struct {
    int x;
    int y;
} Grid;

// 向量结构体
typedef struct {
    float x;
    float y;
} Vec;

// 墙结构体
typedef struct {
    Grid first;
    Grid second;
} Wall;

// 迷宫结构体
typedef struct {
    int map[MAX_GRID_ID][MAX_GRID_ID];
    int vis[HORIZON_GRID_NUMBER][VERTICAL_GRID_NUMBER];
} Maze;

// 获取网格ID
int grid_id(Grid g) {
    return g.y * HORIZON_GRID_NUMBER + g.x;
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

// 生成迷宫（Prim遍历墙算法，https://www.bilibili.com/video/BV1eJ411k7XV）
void maze_generate(Maze* maze) {
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
            int id1 = grid_id(wall.first); // 第i列第j行网格的id为：i + HORIZON_GRID_NUMBER*j，譬如5行3列的地图，总计15个网格，第1行1列的网格就是第0个网格，最后一行最后一列的网格就是第14个网格
            int id2 = grid_id(wall.second);
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
                int current_id = grid_id(wall.second);
                int next_id = grid_id(next);
                
                if (maze->map[current_id][next_id] == 0) {
                    walls[wall_count++] = (Wall){wall.second, next};
                }
            }
        }
    }
    
    free(walls);
}

// 获取墙壁位置
typedef struct {
    Vec start;
    Vec end;
} Block;

Block* get_block_positions(Maze* maze, int* block_count) {
    Block* blocks = malloc(sizeof(Block) * HORIZON_GRID_NUMBER * VERTICAL_GRID_NUMBER * 2);
    *block_count = 0;
    
    // 检查垂直墙壁
    for (int y = 0; y < VERTICAL_GRID_NUMBER; y++) {
        for (int x = 0; x < HORIZON_GRID_NUMBER - 1; x++) {
            Grid g1 = {x, y};
            Grid g2 = {x + 1, y};
            if (maze->map[grid_id(g1)][grid_id(g2)] == 0) {
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
            if (maze->map[grid_id(g1)][grid_id(g2)] == 0) {
                blocks[*block_count].start = (Vec){x * GRID_SIZE, (y + 1) * GRID_SIZE};
                blocks[*block_count].end = (Vec){(x + 1) * GRID_SIZE, (y + 1) * GRID_SIZE};
                (*block_count)++;
            }
        }
    }
    
    return blocks;
}

// 更直观的网格墙打印函数
void print_maze_walls(Maze* maze) {
    printf("Maze Wall Visualization:\n");
    
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
                printf("%s", maze->map[grid_id(current)][grid_id(right)] ? " " : "|");
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
                printf("%s", maze->map[grid_id(current)][grid_id(below)] ? "   " : "---");
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

int main() {
    srand(time(NULL));
    
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