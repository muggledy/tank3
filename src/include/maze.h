#ifndef __MAZE_H__
    #define __MAZE_H__

#include "global.h"

// 定义常量
#define HORIZON_GRID_NUMBER 8
#define VERTICAL_GRID_NUMBER 7
#define GRID_SIZE 80 // 网格数以及网格尺寸的设置是根据窗口大小来的（见gui_tank.c#init_gui()）
#define MAX_GRID_ID (HORIZON_GRID_NUMBER * VERTICAL_GRID_NUMBER)

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

// 获取墙壁位置
typedef struct {
    Vec start;
    Vec end;
} Block;

extern void maze_init(Maze* maze);
extern void maze_generate(Maze* maze);
extern Block* get_block_positions(Maze* maze, tk_uint16_t* block_count);
extern void print_maze_walls(Maze* maze);
extern int grid_id(Grid *g);
extern int is_grid_valid(Grid *g);

#define MAZE_DIAGONAL_GRID -1
#define MAZE_UNRELATED_GRID 0
#define MAZE_ADJACENT_GRID  1
#define MAZE_SAME_GRID      2
extern int is_two_grids_adjacent(Grid *g1, Grid *g2);
extern int is_two_grids_connected(Maze* maze, Grid *g1, Grid *g2);
extern int is_two_grids_the_same(Grid *g1, Grid *g2);

#endif