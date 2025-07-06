#ifndef __GLOBAL_H__
    #define __GLOBAL_H__

#define true  1
#define false 0

#define SET_FLAG(obj, field, flag) (obj)->field |= (flag)
#define CLR_FLAG(obj, field, flag) (obj)->field &= (~(flag))
#define TST_FLAG(obj, field, flag) ((obj)->field & (flag))

#define SET_FLAG2(field, flag) (field) |= (flag)
#define CLR_FLAG2(field, flag) (field) &= (~(flag))
#define TST_FLAG2(field, flag) ((field) & (flag))

#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))

typedef unsigned char  tk_uint8_t;   //0 to 255
typedef signed char    tk_int8_t;    //-128 to +127
typedef unsigned short tk_uint16_t;  //0 to 65535, type int also 16bits
typedef signed short   tk_int16_t;   //-32768 to +32767
typedef unsigned long  tk_uint32_t;  //0 to 4294967295
typedef signed long    tk_int32_t;   //-2147483648 to +2147483647
typedef float          tk_float32_t; //32bits, also is double

#define DEFAULT_FONT_PATH "./assets/Microsoft_JhengHei.ttf"

#endif