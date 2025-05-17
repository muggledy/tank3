#include <SDL2/SDL.h>
#include <math.h>
#include "global.h"

// SDL相关变量
static SDL_Window*   tk_window = NULL;
static SDL_Renderer* tk_renderer = NULL;

// 颜色定义
// 定义颜色枚举
typedef enum {
    TK_WHITE,
    TK_BLACK,
    TK_RED,
    TK_GREEN,
    TK_BLUE,
    TK_YELLOW
} TKColorID;

// 颜色数组
static SDL_Color tk_colors[] = {
    [TK_WHITE]  = {255, 255, 255, 255},
    [TK_BLACK]  = {0, 0, 0, 255},
    [TK_RED]    = {255, 0, 0, 255},
    [TK_GREEN]  = {0, 255, 0, 255},
    [TK_BLUE]   = {0, 0, 255, 255},
    [TK_YELLOW] = {255, 255, 0, 255}
    // 其他颜色...
};

#define ID2COLOR(colorid)    tk_colors[colorid]
#define ID2COLORPTR(colorid) &(tk_colors[colorid])

#define COLOR2PARAM(color) \
    (color).r,(color).g,(color).b,(color).a
#define COLORPTR2PARAM(colorptr) \
    (colorptr)->r,(colorptr)->g,(colorptr)->b,\
    (colorptr)->a
#define COLORPTR2PARAM2(colorptr,alpha) \
    (colorptr)->r,(colorptr)->g,(colorptr)->b,\
    (int)(((colorptr)->a)*(alpha))

#define SET_COLOR(color, r, g, b, a) do { \
    (color).r = (r); \
    (color).g = (g); \
    (color).b = (b); \
    (color).a = (a); \
} while(0)

// 2D向量结构
typedef struct __attribute__((packed)) {
    tk_float32_t x;
    tk_float32_t y;
} Vector2;

typedef Vector2 Point;

typedef struct __attribute__((packed)) {
	Point lefttop;
	Point righttop;
	Point rightbottom;
	Point leftbottom;
} Rectangle;

// 坦克结构
typedef struct {
    tk_uint32_t id;
    Point position;     //坦克中心点
#define TANK_LENGTH 29
#define TANK_WIDTH  23
    tk_float32_t angle_deg; // 朝向角度
    tk_float32_t speed; // 移动速度
#define TANK_INIT_SPEED 2
    tk_uint16_t health; // 生命值
    tk_uint16_t score;  // 分数
} Tank;

#define POS(point) point.x,point.y

// 初始化GUI
void init_gui(void) {
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    // 创建窗口
    tk_window = SDL_CreateWindow("坦克游戏", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                             800, 600, SDL_WINDOW_SHOWN);
    if (tk_window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    // 创建渲染器
    tk_renderer = SDL_CreateRenderer(tk_window, -1, SDL_RENDERER_ACCELERATED);
    if (tk_renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    // 设置渲染器绘制颜色为白色
    SDL_SetRenderDrawColor(tk_renderer, 255, 255, 255, 255);
}

// 清理GUI资源
void cleanup_gui(void) {
    // 销毁渲染器
    if (tk_renderer) {
        SDL_DestroyRenderer(tk_renderer);
        tk_renderer = NULL;
    }

    // 销毁窗口
    if (tk_window) {
        SDL_DestroyWindow(tk_window);
        tk_window = NULL;
    }

    // 退出SDL子系统
    SDL_Quit();
}

// 绕指定点pivot旋转一个点point
static Point rotate_point(const Point *point, tk_float32_t angle, const Point *pivot) {
    if (0 == angle) {
        return (Point){point->x, point->y};
    }
	/*对于一个点 (x, y) 绕中心点 (cx, cy) 旋转 θ 角度后的新坐标 (x', y') 为：
	x' = cx + (x - cx) * cosθ - (y - cy) * sinθ
	y' = cy + (x - cx) * sinθ + (y - cy) * cosθ*/
    // 平移到原点
    tk_float32_t translated_x = point->x - pivot->x;
    tk_float32_t translated_y = point->y - pivot->y;
    /* 计算旋转矩阵 R = [ cosθ  -sinθ ]
					  [ sinθ   cosθ ]
	所需的三角函数值 */
    tk_float32_t cos_angle = cosf(angle);
    tk_float32_t sin_angle = sinf(angle);
    // 应用旋转矩阵：R·([x,y]^T)
    tk_float32_t rotated_x = translated_x * cos_angle - translated_y * sin_angle;
    tk_float32_t rotated_y = translated_x * sin_angle + translated_y * cos_angle;
    // 平移回原位置
    return (Point){rotated_x + pivot->x, rotated_y + pivot->y};
}

// 计算旋转后的点
static void calculate_rotated_rectangle(const Point *center, tk_float32_t width, tk_float32_t height, tk_float32_t angle_deg, Rectangle *rect) {
    tk_float32_t angle = angle_deg * (M_PI / 180.0f);  // 将角度转换为弧度
    uint8_t i = 0;
    // 未旋转时矩形的四个顶点坐标
    Point points[4] = {
        {center->x - (width/2), center->y - (height/2)},
        {center->x + (width/2), center->y - (height/2)},
        {center->x + (width/2), center->y + (height/2)},
		{center->x - (width/2), center->y + (height/2)}
    };
	Point *new_points = (Point *)rect;

    // 旋转每个顶点
    for (i = 0; i < 4; i++) {
        new_points[i] = rotate_point(&(points[i]), angle, center);
    }
}

Rectangle draw_rectangle(SDL_Renderer* renderer, const Point *center, 
        tk_float32_t width, tk_float32_t height, tk_float32_t angle_deg, SDL_Color *color) {
    /*East: angle_deg == 0*/
    Rectangle rect;
    calculate_rotated_rectangle(center, width, height, angle_deg, &rect);
    SDL_SetRenderDrawColor(renderer, COLORPTR2PARAM(color));

    // 索引数组，定义两个三角形组成矩形
    SDL_Vertex vertices[4] = {
        {{POS(rect.lefttop)},     {COLORPTR2PARAM(color)}, {0, 0}},
        {{POS(rect.righttop)},    {COLORPTR2PARAM(color)}, {1, 0}},
        {{POS(rect.rightbottom)}, {COLORPTR2PARAM(color)}, {1, 1}},
        {{POS(rect.leftbottom)},  {COLORPTR2PARAM(color)}, {0, 1}}
    };
    // printf("p1:(%f,%f),p2:(%f,%f),p3:(%f,%f),p4:(%f,%f)\n", 
    //     POS(rect.lefttop), POS(rect.righttop), POS(rect.rightbottom), POS(rect.leftbottom));
    // 定义两个三角形的顶点索引
    const int indices[] = {0,1,2, 2,3,0};

    // 调用SDL_RenderGeometry绘制矩形
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); //支持透明度
    SDL_RenderGeometry(renderer, NULL, vertices, 4, indices, 6);
    return rect;
}

static Point get_line_center(const Point *p1, const Point *p2) {
    Point center;
    center.x = ((p1->x + p2->x) / 2);
    center.y = ((p1->y + p2->y) / 2);
    return center;
}

static Point get_line_k_center(const Point *p1, const Point *p2, double k) { //(k_center - p1) / (p2 - p1) = k
    Point k_center;
    k_center.x = ((p2->x - p1->x) * k + p1->x);
    k_center.y = ((p2->y - p1->y) * k + p1->y);
    return k_center;
}

// 渲染坦克
static void render_tank(const Tank* tank, int is_player) {
    /* lefttop     righttop
         ↖︎         ↗
           ▛▀▀▀▀▀▜
    TANK_  ▌   C⏹︎⏹︎⏹︎⏹︎   North: angle_deg == 0. rotate clockwise,
    WIDTH  ▙▄▄▄▄▄▟      angle_deg should range in [0, 360]
          TANK_LENGTH
        C: tank->position
    */
    Rectangle rect;
    // 设置坦克颜色（玩家坦克为蓝色，AI坦克为红色）
    SDL_Color *color = is_player ? ID2COLORPTR(TK_BLUE) : ID2COLORPTR(TK_RED);
    SDL_Color body_color = (SDL_Color){COLORPTR2PARAM2(color, 0.5)};
    Point topline_center;
    Point bottomline_center;
    Point rightline_center;
    Point gun_barrel_center;

    tk_float32_t angle_deg = tank->angle_deg;
    if (angle_deg < 0) {
        angle_deg = 0;
    }
    if (angle_deg > 360) {
        angle_deg = 360;
    }
    angle_deg += 270;
    if (angle_deg >= 360) {
        angle_deg -= 360;
    }
    // 绘制坦克主体
    rect = draw_rectangle(tk_renderer, &(tank->position), TANK_LENGTH, TANK_WIDTH, angle_deg, &body_color);

    // 绘制履带
    topline_center = get_line_center(&rect.lefttop, &rect.righttop);
    bottomline_center = get_line_center(&rect.leftbottom, &rect.rightbottom);
    draw_rectangle(tk_renderer, &topline_center, TANK_LENGTH-4, 4, angle_deg, color);
    draw_rectangle(tk_renderer, &bottomline_center, TANK_LENGTH-4, 4, angle_deg, color);

    //绘制炮塔
    draw_rectangle(tk_renderer, &(tank->position), 15, 15, angle_deg, color);
    rightline_center = get_line_center(&rect.righttop, &rect.rightbottom);
    gun_barrel_center = get_line_k_center(&(tank->position), &rightline_center, 0.8);
    draw_rectangle(tk_renderer, &gun_barrel_center, 18, 9, angle_deg, color);

    // // 绘制坦克生命值
    // SDL_SetRenderDrawColor(renderer, COLOR2PARAM(ID2COLOR(ID_GREEN)));
    // SDL_Rect health_bar = {
    //     (int)tank->position.x - 20,
    //     (int)tank->position.y - 30,
    //     tank->health * 40 / 100, 5
    // };
    // SDL_RenderFillRect(renderer, &health_bar);
}

// 渲染场景
static void render_scene(const Tank* tank) {
    // 清空屏幕
    SDL_SetRenderDrawColor(tk_renderer, COLOR2PARAM(ID2COLOR(TK_WHITE)));
    SDL_RenderClear(tk_renderer);

    // 渲染坦克
    render_tank(tank, 0);

    // 显示渲染内容
    SDL_RenderPresent(tk_renderer);
}

// 计算从给定点沿着指定方向移动指定距离后的新坐标
Point move_point(Point start, tk_float32_t direction, tk_float32_t distance) {
    /*North: direction == 0*/
	if (direction < 0) {
        direction = 0;
    }
    if (direction > 360) {
        direction = 360;
    }
    direction = 360 - direction + 90;
	if (direction >= 360) {
        direction -= 360;
    }

	// 将角度转换为弧度
    tk_float32_t direction_rad = direction * M_PI / 180.0f;
    // 计算新坐标
    Point end;
    end.x = start.x + distance * cosf(direction_rad);
    end.y = start.y - distance * sinf(direction_rad);
    return end;
}

int main() {
    int quit = 0;
    SDL_Event e;
    Tank tank = {1, {400,300}, 300, TANK_INIT_SPEED, 100, 80}; // 居中放置坦克
    tk_float32_t new_dir = 0;

    init_gui();
    // 主循环
    while (!quit) {
        // 处理事件
        while (SDL_PollEvent(&e) != 0) {
            // 用户请求退出
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
            // 处理键盘事件
            else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = 1;
                        break;
                    case SDLK_w:
                        tank.position = move_point(tank.position, tank.angle_deg, tank.speed);
                        break;
                    case SDLK_s:
                        new_dir = tank.angle_deg + 180;
                        if (new_dir > 360) {
                            new_dir -= 360;
                        }
                        tank.position = move_point(tank.position, new_dir, tank.speed);
                        break;
                    case SDLK_a:
                        tank.angle_deg += 360;
                        tank.angle_deg -= 10;
                        if (tank.angle_deg >= 360) {
                            tank.angle_deg -= 360;
                        }
                        break;
                    case SDLK_d:
                        tank.angle_deg += 10;
			            if (tank.angle_deg >= 360) {
                            tank.angle_deg -= 360;
                        }
                        break;
                }
            }
        }

        // 渲染场景
        render_scene(&tank);

        // 控制帧率
        SDL_Delay(30); // 约60FPS
    }

    cleanup_gui();
    return 0;
}