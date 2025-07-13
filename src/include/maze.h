#ifndef __MAZE_H__
    #define __MAZE_H__

#include "global.h"
#include <pthread.h>

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

typedef struct {
    Grid current;  //当前网格坐标
    Grid previous; //到达当前网格的前一个网格坐标，据此可以上溯至最开始的起点
    tk_uint8_t is_current_visited; //当前网格是否已被访问过
    tk_uint16_t steps; //距离起点的步数
} MazePathBFSearchNode;

/*基于BFS广度优先遍历的最短路径搜索管理器*/
typedef struct {
    Grid start; //起点
    Grid end;   //终点
    Maze *maze; //游戏地图
    MazePathBFSearchNode maze_node_status_tbl[HORIZON_GRID_NUMBER][VERTICAL_GRID_NUMBER];
    MazePathBFSearchNode* bfs_queue[HORIZON_GRID_NUMBER*VERTICAL_GRID_NUMBER];
    tk_uint16_t front; //front游标用于迭代bfs_queue中的元素，直至没有元素(front>=rear)可供迭代或者找到了终点，广度优先搜索结束
    tk_uint16_t rear;  //rear游标用于指示放入bfs_queue中的有效元素的末尾位置
    void (*bfs_search)(void*); //入参就是当前Manager管理器对象
    tk_uint8_t success; //搜索是否成功标记
    pthread_spinlock_t spinlock; //GUI线程访问搜索结果以及控制线程计算搜索路径可能并行，需要加锁
} MazePathBFSearchManager;

#if 0
#define FOREACH_BFS_SEARCH_MANAGER_GRID(manager) \
    for (Grid next = (manager)->end, *_loop_once = NULL; \
         !_loop_once && (manager)->success; \
         _loop_once = (void*)1) \
        for (int i = 0; \
             (manager)->success && (i <= (manager)->maze_node_status_tbl[(manager)->end.x][(manager)->end.y].steps); \
             i++, next = (manager)->maze_node_status_tbl[next.x][next.y].previous)
#endif
/*遍历最短路径上的网格位置（逆序：终点->起点）*/
#define FOREACH_BFS_SEARCH_MANAGER_GRID(manager, next) \
    next = (manager)->end; \
    for (int i = 0; \
         (manager)->success && (i <= (manager)->maze_node_status_tbl[(manager)->end.x][(manager)->end.y].steps); \
         i++, next = (manager)->maze_node_status_tbl[next.x][next.y].previous)

#define NEXT_BFS_SEARCH_GRID(manager, current) \
    (manager)->maze_node_status_tbl[current.x][current.y].steps ? (manager)->maze_node_status_tbl[current.x][current.y].previous : \
    (Grid){-1, -1}

#ifndef POS
#define POS(grid) (grid).x,(grid).y
#endif

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

extern void bfs_shortest_path_search(void *maze_path_bfs_search_manager);

#endif