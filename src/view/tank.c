#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "tank.h"

px_color color_palette[][2] = {
	{{178,0,0, 255}, {255,76,76, 255}}, //tank gun barrel and tank body's color
};

Point rotate_point(Point point, double angle, Point pivot) { // 绕指定点pivot旋转一个点point
	/*对于一个点 (x, y) 绕中心点 (cx, cy) 旋转 θ 角度后的新坐标 (x', y') 为：
	x' = cx + (x - cx) * cosθ - (y - cy) * sinθ
	y' = cy + (x - cx) * sinθ + (y - cy) * cosθ*/
    // 平移到原点
    double translated_x = point.x - pivot.x;
    double translated_y = point.y - pivot.y;
    /* 计算旋转矩阵 R = [ cosθ  -sinθ ]
					  [ sinθ   cosθ ]
	所需的三角函数值 */
    double cos_angle = cos(angle);
    double sin_angle = sin(angle);
    // 应用旋转矩阵：R·([x,y]^T)
    double rotated_x = translated_x * cos_angle - translated_y * sin_angle;
    double rotated_y = translated_x * sin_angle + translated_y * cos_angle;
    // 平移回原位置（四舍五入取整）
    Point rotated_point;
    rotated_point.x = (int)round(rotated_x + (double)pivot.x);
    rotated_point.y = (int)round(rotated_y + (double)pivot.y);

    return rotated_point;
}

void calculate_rotated_rectangle(Point pos1, double width, double height, double angle_deg, Rectangle *rect) { //计算旋转后矩形的四个顶点坐标
    double angle = angle_deg * (M_PI / 180);  // 将角度转换为弧度
    // 未旋转时矩形的四个顶点坐标
    Point points[4] = {
        {pos1.x, pos1.y},
        {pos1.x + width, pos1.y},
        {pos1.x + width, pos1.y + height},
		{pos1.x, pos1.y + height}
    };
	Point *result = (Point *)rect;

    // 旋转每个顶点
    for (int i = 0; i < 4; i++) {
        result[i] = rotate_point(points[i], angle, pos1);
		// printf("Vertex %d: (%.2f, %.2f)\n", i + 1, result[i].x, result[i].y);
    }
}

#if 1
bool is_point_inside_polygon(Point p, Point polygon[], int n) { // 判断点(参数p)是否在由四(参数n)个顶点构成的多边形(参数polygon)内（使用射线法）
	/*使用射线法判断一个点是否在由四个顶点构成的多边形（即旋转后的矩形）内。射线法的基本思想是从该点向右发射一条射线，
	统计射线与多边形边的交点个数，如果交点个数为奇数，则点在多边形内；如果为偶数，则点在多边形外*/
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) { //依次遍历的边线：[(point0, point3), (point1, point0), (point2, point1), (point3, point2)]
        if (((polygon[i].y > p.y) != (polygon[j].y > p.y)) &&
            (p.x < (polygon[j].x - polygon[i].x) * (p.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}
#else
bool is_point_inside_polygon(Point p, Point polygon[], int n) { // 判断点是否在由四个顶点构成的多边形内（使用射线法，增加对特殊情况的处理）
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = (double)polygon[i].x, yi = (double)polygon[i].y;
        double xj = (double)polygon[j].x, yj = (double)polygon[j].y;
        double px = (double)p.x, py = (double)p.y;

        bool intersect = ((yi > py) != (yj > py)) &&
                         (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
        // 处理射线经过顶点的情况
        if (yi == py && yj == py) {
            if (xi <= px && px <= xj || xj <= px && px <= xi) {
                intersect = true;
            }
        }
        if (intersect) {
            inside =!inside;
        }
    }
    return inside;
}
#endif

void scanline_traversal_rectangle_and_fill(Rectangle *rect, px_color color) { // 扫描线算法遍历旋转矩形内的点（TODO：如何抗锯齿？）
	Point *vertices = (Point *)rect;
    int min_x = vertices[0].x, max_x = vertices[0].x, min_y = vertices[0].y, max_y = vertices[0].y;
	Point p = {0, 0};
	px_surface* surface = PX_Object_PanelGetSurface(App.object_printer);

    for (int i = 1; i < 4; i++) { // 找到矩形所在的最大边界范围
        if (vertices[i].x < min_x) min_x = vertices[i].x;
        else if (vertices[i].x > max_x) max_x = vertices[i].x;
        if (vertices[i].y < min_y) min_y = vertices[i].y;
        else if (vertices[i].y > max_y) max_y = vertices[i].y;
    }

	// 扩大一圈扫描范围，避免出现边缘点因为round四舍五入而遗漏
    // min_x--;
    // max_x++;
    // min_y--;
    // max_y++;
	// printf("min_x:%d, min_y:%d, max_x:%d, max_y:%d\n", min_x,min_y,max_x,max_y);

    for (p.y = min_y; p.y <= max_y; p.y++) { // 逐行扫描
        for (p.x = min_x; p.x <= max_x; p.x++) {
            if (is_point_inside_polygon(p, vertices, 4)) {
                //printf("Point (%d, %d) is inside the rectangle.\n", p.x, p.y);
				PX_SurfaceDrawPixel(surface, POINT2POS(p), color);
            }
        }
    }
}

Rectangle PainterEngine_DrawSolidRect(px_int x,px_int y,px_int width,px_int height,px_double angle_deg,px_color fill_color)
{
	Rectangle rect;
	Point lefttop_pos;
	if (App.object_printer->Visible == PX_FALSE)
	{
		PX_ObjectSetVisible(App.object_printer, PX_TRUE);
	}
	lefttop_pos.x = x;
	lefttop_pos.y = y;
    calculate_rotated_rectangle(lefttop_pos, width, height, angle_deg, &rect);
	px_color line_color = fill_color; //PX_COLOR(255, 255, 0, 0);
	line_color._argb.a -= 80;
	PainterEngine_DrawLine(POINT2POS(rect.lefttop), POINT2POS(rect.righttop), 1, line_color); //绘制边线，这是为了抗锯齿的临时之举
	PainterEngine_DrawLine(POINT2POS(rect.lefttop), POINT2POS(rect.leftbottom), 1, line_color);
	PainterEngine_DrawLine(POINT2POS(rect.leftbottom), POINT2POS(rect.rightbottom), 1, line_color);
	PainterEngine_DrawLine(POINT2POS(rect.righttop), POINT2POS(rect.rightbottom), 1, line_color);

	scanline_traversal_rectangle_and_fill(&rect, fill_color);
	return rect;
}

Point get_rectangle_center(const Rectangle *rect) {
    Point center;
    center.x = (int)round((rect->lefttop.x + rect->rightbottom.x) / 2);
    center.y = (int)round((rect->lefttop.y + rect->rightbottom.y) / 2);
    return center;
}

Point get_line_center(const Point *p1, const Point *p2) {
    Point center;
    center.x = (int)round((p1->x + p2->x) / 2);
    center.y = (int)round((p1->y + p2->y) / 2);
    return center;
}

Point get_line_k_center(const Point *p1, const Point *p2, double k) { //(k_center - p1) / (p2 - p1) = k
    Point k_center;
    k_center.x = (int)round((p2->x - p1->x) * k + p1->x);
    k_center.y = (int)round((p2->y - p1->y) * k + p1->y);
    return k_center;
}

double get_length_of_two_points(const Point *p1, const Point *p2) {
	double dx = p2->x - p1->x;
    double dy = p2->y - p1->y;
    return sqrt(dx * dx + dy * dy);
}

int draw_tank() {
	Point tank_lefttop = {200, 200};
	px_double angle_deg = 0;
	px_color color = color_palette[PALETTE_RED][0], color2 = color_palette[PALETTE_RED][1];
	Rectangle rect;
	Point rect_center, topline_center, gun_barrel_lefttop;
	rect = PainterEngine_DrawSolidRect(POINT2POS(tank_lefttop), 27, 20, angle_deg, color2);
	PainterEngine_DrawLine(POINT2POS(rect.lefttop), POINT2POS(rect.righttop), 3, color);
	PainterEngine_DrawLine(POINT2POS(rect.leftbottom), POINT2POS(rect.rightbottom), 3, color);
	rect_center = get_rectangle_center(&rect);
	PainterEngine_DrawSolidCircle(POINT2POS(rect_center), 9, color);
	topline_center = get_line_center(&rect.lefttop, &rect.righttop);
	gun_barrel_lefttop = get_line_k_center(&rect_center, &topline_center, 0.46);
	PainterEngine_DrawSolidRect(POINT2POS(gun_barrel_lefttop), 18, (int)round(get_length_of_two_points(&rect_center,&gun_barrel_lefttop)*2), 
		angle_deg, color);
    return 0;
}
