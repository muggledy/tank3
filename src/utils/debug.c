#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <sys/syscall.h>  // 获取更短的线程ID
#include <unistd.h>       // 提供 SYS_gettid 和 pid_t 的定义
#include "debug.h"

#define DEBUG_BUF_SIZE 1024

// 线程局部存储的前缀缓冲
static __thread char prefix_buf[64] = {0};
// 线程本地存储的完整输出缓冲区
static __thread char output_buf[DEBUG_BUF_SIZE];

void tk_debug_internal(int control, const char *format, ...) {
    if (!control) return;
    // 初始化线程本地前缀（每个线程只执行一次）
    if (prefix_buf[0] == '\0') {
        pid_t tid = syscall(SYS_gettid);  // 获取内核级线程ID
        snprintf(prefix_buf, sizeof(prefix_buf), "[T-%d] ", tid);
    }

    va_list args;
    va_start(args, format);

    // 合并前缀和用户内容到输出缓冲区
    size_t prefix_len = strlen(prefix_buf);
    strncpy(output_buf, prefix_buf, prefix_len);
    output_buf[DEBUG_BUF_SIZE - 1] = '\0';
    // 追加格式化内容
    vsnprintf(output_buf + prefix_len, 
              DEBUG_BUF_SIZE - prefix_len - 1, 
              format, args);

    va_end(args);

    // 一次性输出完整内容
    fputs(output_buf, stdout);

    // 可选：立即刷新
    fflush(stdout);
}

// 可选操作，重置线程前缀打印标识字符串
void reset_debug_prefix(char *prefix) {
    snprintf(prefix_buf, sizeof(prefix_buf), "[T-%s] ", prefix);
}