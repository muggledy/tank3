#include "tank.h"

int main() {
	PainterEngine_Initialize(800,580);
	Point pos = {200, 200};
	px_double angle_deg = 200;
	int palette_color_id = PALETTE_BLACK;
	draw_tank(pos, angle_deg, palette_color_id);
    return 0;
}
