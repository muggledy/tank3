#ifndef __IDPOOL_H__
    #define __IDPOOL_H__

#include <stdint.h>
#include <stddef.h>

#define ID_POOL_SIZE 1024  // 最大ID数量，可根据需要调整

typedef struct {
    uint32_t *bitmap;       // bitmap数组
    size_t size;            // ID池大小(实际可用的ID数量)
    size_t last_alloc_pos;  // 上次分配位置，用于优化搜索
    int max_id;             // 最大ID值
} IDPool;

extern IDPool* id_pool_create(size_t max_id);
extern void id_pool_destroy(IDPool *pool);
extern int id_pool_allocate(IDPool *pool);
extern void id_pool_release(IDPool *pool, int id);
extern void id_pool_print(IDPool *pool);
#define print_id_pool id_pool_print

#endif