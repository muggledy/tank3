#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "debug.h"

#define PATH_MAX_LEN 512
#define THREAD_LOCAL __thread

char* get_absolute_path(char *relative_path) {
    static THREAD_LOCAL char absolute_path[PATH_MAX_LEN];

    if (realpath(relative_path, absolute_path)) {
        tk_debug("%s's absolute path is %s\n", relative_path, absolute_path);
    } else {
        snprintf(absolute_path, sizeof(absolute_path), "Error(get `%s` absolute path)", relative_path);
        perror(absolute_path);
        return NULL;
    }

    return absolute_path;
}

// 正整数转字符串，使用 static 缓冲区
char* uint_to_str(unsigned int num) {
    static THREAD_LOCAL char str[12]; // 足够存储 32 位整数（包括 '\0'）
    int i = 0;

    // 特殊情况：0
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }

    // 从低位到高位提取数字
    while (num > 0) {
        str[i++] = (num % 10) + '0'; // 数字转 ASCII
        num /= 10;
    }
    str[i] = '\0';

    // 反转字符串（因为数字是逆序存储的）
    for (int left = 0, right = i - 1; left < right; left++, right--) {
        char tmp = str[left];
        str[left] = str[right];
        str[right] = tmp;
    }

    return str;
}