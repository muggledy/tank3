#include <stdio.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <sys/syscall.h>  // 获取更短的线程ID
#include <unistd.h>       // 提供 SYS_gettid 和 pid_t 的定义

// 线程局部存储缓冲
static __thread char prefix_buf[64];

void tk_debug(const char *format, ...) {
    // 初始化线程本地前缀（每个线程只执行一次）
    if (prefix_buf[0] == '\0') {
        pid_t tid = syscall(SYS_gettid);  // 获取内核级线程ID
        snprintf(prefix_buf, sizeof(prefix_buf), "[T-%d] ", tid);
    }
    
    // 打印前缀
    fputs(prefix_buf, stdout);
    
    // 打印用户内容
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    // 可选：立即刷新
    fflush(stdout);
}

// 可选操作，重置线程前缀打印标识字符串
void reset_debug_prefix(char *prefix) {
    snprintf(prefix_buf, sizeof(prefix_buf), "[T-%s] ", prefix);
}