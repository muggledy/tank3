#ifndef __TOOLS_H__
    #define __TOOLS_H__

#include <stddef.h>

extern char* get_absolute_path(char *relative_path);
extern char* uint_to_str(unsigned int num);
extern int random_range(int m, int n);
extern size_t strlcpy(char *dst, const char *src, size_t size);

#endif