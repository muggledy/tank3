#ifndef TANK_H__
    #define TANK_H__

#include "PainterEngine.h"

#define draw_tank draw_tank_v1

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
	PALETTE_BLACK,
} color_palette_type;

#define SetPosition(pos,_x,_y) \
do { \
    (pos).x = (_x); \
    (pos).y = (_y); \
} while(0)

static Point round_point(Point pos) {
	Point ret;
	SetPosition(ret, (int)round(pos.x), (int)round(pos.y));
	return ret;
}

extern Rectangle PainterEngine_DrawSolidRect(px_int x,px_int y,px_int width,px_int height,px_double angle_deg,px_color fill_color);
extern void draw_tank_v0(Point tank_lefttop, px_double angle_deg, int palette_color_id);
extern void draw_tank_v1(Point tank_center, px_double angle_deg, int palette_color_id);
extern Point move_point(Point start, px_float direction, px_float distance);

#endif