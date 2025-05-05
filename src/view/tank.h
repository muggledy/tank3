#ifndef TANK_H__
    #define TANK_H__

#include "PainterEngine.h"

extern px_color color_palette[][2];

#define POS(x, y) (x),(y)

typedef struct __attribute__((packed)) {
    px_float x;
    px_float y;
} Position;

typedef Position Point;

typedef struct __attribute__((packed)) {
	Point lefttop;
	Point righttop;
	Point rightbottom;
	Point leftbottom;
} Rectangle;

#define POINT2POS(point) (point).x,(point).y

typedef enum {
	PALETTE_RED = 0,
} color_palette_type;

extern Rectangle PainterEngine_DrawSolidRect(px_int x,px_int y,px_int width,px_int height,px_double angle_deg,px_color fill_color);
extern int draw_tank();

#endif