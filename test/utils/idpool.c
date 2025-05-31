#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "idpool.h"
#include "debug.h"

#define BITS_PER_WORD (sizeof(uint32_t) * CHAR_BIT)
#define MIN_ID 1           // 最小ID值

// 初始化ID池
IDPool* id_pool_create(size_t max_id) {
    if (max_id < MIN_ID) max_id = MIN_ID;
    
    IDPool *pool = (IDPool*)malloc(sizeof(IDPool));
    if (!pool) return NULL;
    
    pool->max_id = max_id;
    pool->size = max_id - MIN_ID + 1;  // 实际需要管理的ID范围
    pool->last_alloc_pos = 0;
    
    // 计算需要的bitmap数组大小
    size_t bitmap_size = (pool->size + BITS_PER_WORD - 1) / BITS_PER_WORD;
    pool->bitmap = (uint32_t*)calloc(bitmap_size, sizeof(uint32_t));
    
    if (!pool->bitmap) {
        free(pool);
        return NULL;
    }
    
    return pool;
}

// 销毁ID池
void id_pool_destroy(IDPool *pool) {
    if (pool) {
        free(pool->bitmap);
        free(pool);
    }
}

// 分配一个ID (返回值为MIN_ID到max_id)
int id_pool_allocate(IDPool *pool) {
    if (!pool) return 0;  // 返回0表示失败
    
    size_t bitmap_size = (pool->size + BITS_PER_WORD - 1) / BITS_PER_WORD;
    size_t start_word = pool->last_alloc_pos / BITS_PER_WORD;
    
    // 从上次分配位置开始查找空闲ID
    for (size_t i = 0; i < bitmap_size; i++) {
        size_t word_idx = (start_word + i) % bitmap_size;
        uint32_t word = pool->bitmap[word_idx];
        
        // 如果当前word有空闲位
        if (word != UINT32_MAX) {
            // 查找第一个为0的位
            for (size_t bit = 0; bit < BITS_PER_WORD; bit++) {
                size_t pos = word_idx * BITS_PER_WORD + bit;
                if (pos >= pool->size) break;  // 超出范围
                
                if (!(word & (1 << bit))) {
                    pool->bitmap[word_idx] |= (1 << bit);
                    pool->last_alloc_pos = pos;
                    return (int)(pos + MIN_ID);  // 转换为实际ID
                }
            }
        }
    }
    
    return 0;  // 没有可用ID
}

// 释放一个ID
void id_pool_release(IDPool *pool, int id) {
    if (!pool || id < MIN_ID || id > pool->max_id) return;
    
    size_t pos = (size_t)(id - MIN_ID);
    size_t word_idx = pos / BITS_PER_WORD;
    size_t bit = pos % BITS_PER_WORD;
    
    pool->bitmap[word_idx] &= ~(1 << bit);
}

// 打印ID池状态(调试用)
void id_pool_print(IDPool *pool) {
    if (!pool) return;
    
    tk_debug("ID Pool Status (Range: %d-%d):\n", MIN_ID, pool->max_id);
    for (size_t i = 0; i < pool->size; i++) {
        size_t word_idx = i / BITS_PER_WORD;
        size_t bit = i % BITS_PER_WORD;
        
        if (i % 64 == 0) printf("\n%04zu: ", i + MIN_ID);
        printf("%d", (pool->bitmap[word_idx] & (1 << bit)) ? 1 : 0);
    }
    printf("\n");
}

#if 0
// 示例用法
int main() {
    IDPool *pool = id_pool_create(ID_POOL_SIZE);
    
    // 分配一些ID
    int id1 = id_pool_allocate(pool);
    int id2 = id_pool_allocate(pool);
    int id3 = id_pool_allocate(pool);
    
    printf("Allocated IDs: %d, %d, %d\n", id1, id2, id3);
    
    // 释放一个ID
    id_pool_release(pool, id2);
    printf("Released ID: %d\n", id2);
    
    // 再次分配应该会重用id2
    int id4 = id_pool_allocate(pool);
    printf("Allocated new ID: %d (should be same as released ID %d)\n", id4, id2);
    
    // 打印池状态
    id_pool_print(pool);
    
    id_pool_destroy(pool);
    return 0;
}
#endif