#include <math.h>
#include <time.h>
#include <unistd.h>
#include "tank.h"
#include <bsd/string.h>
#include <stdlib.h>
#include "idpool.h"

// SDL相关变量
static SDL_Window*   tk_window = NULL;
static SDL_Renderer* tk_renderer = NULL;
IDPool *tk_idpool = NULL;

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

// 预定义碎片形状顶点（以中心点为原点）
static const Point particle_shape_vertices[][6] = {
    // 三角形
    {{-2, -4}, {2, -4}, {0, 4}, {0,0}},
    // 四边形（菱形）
    {{-3, 0}, {0, -3}, {3, 0}, {0, 3}, {0,0}},
    // 五边形（简化版）
    {{-3, -2}, {0, -4}, {3, -2}, {2, 3}, {-2, 3}, {0,0}}
};

// 初始化GUI
int init_gui(void) {
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    // 创建窗口
    tk_window = SDL_CreateWindow("坦克-3", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                             800, 600, SDL_WINDOW_SHOWN);
    if (tk_window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -2;
    }

    // 创建渲染器
    tk_renderer = SDL_CreateRenderer(tk_window, -1, SDL_RENDERER_ACCELERATED);
    if (tk_renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return -3;
    }

    // 设置渲染器绘制颜色为白色
    SDL_SetRenderDrawColor(tk_renderer, 255, 255, 255, 255);

    return 0;
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

// 绘制箭头头部（简化版）
static void draw_arrow_head(SDL_Renderer* renderer, Point end, float angle) {
    /*
        \ 夹角θ(144°)
     ----→(end) →:angle所代表的主轴方向(East: 0°, North: 270°)
        /
    */
    float arrow_size = 5.0f;

    float arrow_angle1 = angle + M_PI * 0.8f;  // 箭头一侧角度 angle + M_PI*0.8f ≈ angle + θ(144°)
    float arrow_angle2 = angle - M_PI * 0.8f;  // 箭头另一侧角度

    Point arrow_point1 = {
        .x = end.x + arrow_size * cosf(arrow_angle1),
        .y = end.y + arrow_size * sinf(arrow_angle1) //横轴i单位向量[1,0]逆时针旋转θ后坐标为[cos(θ),sin(θ)]
    };
    Point arrow_point2 = {
        .x = end.x + arrow_size * cosf(arrow_angle2),
        .y = end.y + arrow_size * sinf(arrow_angle2)
    };

    // 绘制箭头线条
    SDL_RenderDrawLine(renderer, end.x, end.y, arrow_point1.x, arrow_point1.y);
    SDL_RenderDrawLine(renderer, end.x, end.y, arrow_point2.x, arrow_point2.y);
}

// 绘制坦克坐标系（北轴和右轴）
void draw_tank_coordinates(SDL_Renderer* renderer, const Tank* tank) {
    if (!renderer || !tank) return;

    // 设置绘制颜色
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // 白色
    // 坐标系原点（坦克中心）
    Point origin = tank->position;

    float north_angle_rad = 270 * M_PI / 180.0f;
    Point north_end = {
        .x = origin.x,  // 北轴长度23像素
        .y = origin.y - 23
    };
    // 计算右轴方向（垂直于北轴，顺时针旋转90度）
    float right_angle_rad = 0 * M_PI / 180.0f;
    Point right_end = {
        .x = origin.x + 20,  // 右轴长度20像素
        .y = origin.y
    };

    // 绘制北轴
    SDL_SetRenderDrawColor(renderer, 75, 193, 169, 255);
    SDL_RenderDrawLine(renderer, origin.x, origin.y, north_end.x, north_end.y);
    draw_arrow_head(renderer, north_end, north_angle_rad);  // 北轴箭头

    // 绘制右轴
    SDL_SetRenderDrawColor(renderer, 75, 193, 169, 255);
    SDL_RenderDrawLine(renderer, origin.x, origin.y, right_end.x, right_end.y);
    draw_arrow_head(renderer, right_end, right_angle_rad);  // 右轴箭头

    // 坦克前进方向轴（不带箭头）
#define SCOPE_LEN 200 // 便于瞄准敌人
    float tank_ang = (tank->angle_deg + 270);
    if (tank_ang > 360) {
        tank_ang -= 360;
    }
    float tank_angle_rad = tank_ang * M_PI / 180.0f;
    Point tank_angle_end = {
        .x = origin.x + SCOPE_LEN * cosf(tank_angle_rad),  // 长度SCOPE_LEN像素
        .y = origin.y + SCOPE_LEN * sinf(tank_angle_rad)
    };
    SDL_SetRenderDrawColor(renderer, COLORPTR2PARAM2(ID2COLORPTR(TK_BLACK), 0.3));
    SDL_RenderDrawLine(renderer, origin.x, origin.y, tank_angle_end.x, tank_angle_end.y);
}

extern void trigger_explode(Tank *tank);
extern void render_explode_effect(SDL_Renderer* renderer, Tank *tank);
extern void update_explode_particles_state(Tank *tank);

// 渲染坦克
void render_tank(SDL_Renderer* renderer, Tank* tank) {
    /* lefttop     righttop
         ↖︎         ↗
           ▛▀▀▀▀▀▜
    TANK_  ▌   C⏹︎⏹︎⏹︎⏹︎   North: angle_deg == 0. rotate clockwise,
    WIDTH  ▙▄▄▄▄▄▟      angle_deg should range in [0, 360]
          TANK_LENGTH
        C: tank->position
    */
    if (!renderer || !tank) {
        return;
    }
    if (TST_FLAG(tank, flags, TANK_DEAD)) {
        return;
    }
    if ((tank->health <= 0) || TST_FLAG(tank, flags, TANK_DYING)) {
        if (TST_FLAG(tank, flags, TANK_ALIVE)) {
            CLR_FLAG(tank, flags, TANK_ALIVE);
            trigger_explode(tank);
            SET_FLAG(tank, flags, TANK_DYING);
        }
        // 绘制爆炸效果
        render_explode_effect(renderer, tank);
        // 更新爆炸粒子
        update_explode_particles_state(tank);
        if (tank->explode_effect.active_count <= 0) {
            CLR_FLAG(tank, flags, TANK_DYING);
            SET_FLAG(tank, flags, TANK_DEAD);
        }
        return;
    }
    Rectangle rect;
    // 设置坦克颜色（玩家坦克为蓝色，AI坦克为红色）
    SDL_Color *color = tank->basic_color;
    SDL_Color body_color = (SDL_Color){COLORPTR2PARAM2(color, 0.5)};
    Point topline_center;
    Point bottomline_center;
    Point rightline_center;
    Point gun_barrel_center;
    Point health_bar_lefttop;
    tk_float32_t life_percentage = 0;

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
    rect = draw_rectangle(renderer, &(tank->position), TANK_LENGTH, TANK_WIDTH, angle_deg, &body_color);

    // 绘制履带
    topline_center = get_line_center(&rect.lefttop, &rect.righttop);
    bottomline_center = get_line_center(&rect.leftbottom, &rect.rightbottom);
    draw_rectangle(renderer, &topline_center, TANK_LENGTH-4, 4, angle_deg, color);
    draw_rectangle(renderer, &bottomline_center, TANK_LENGTH-4, 4, angle_deg, color);

    // 绘制炮塔
    draw_rectangle(renderer, &(tank->position), 15, 15, angle_deg, color);
    rightline_center = get_line_center(&rect.righttop, &rect.rightbottom);
    gun_barrel_center = get_line_k_center(&(tank->position), &rightline_center, 0.8);
    draw_rectangle(renderer, &gun_barrel_center, 18, 9, angle_deg, color);

    // 绘制坦克生命值
    SDL_RenderDrawPoint(renderer, POS(tank->position));
    health_bar_lefttop = (Point){tank->position.x-22, tank->position.y-30};
    SDL_SetRenderDrawColor(renderer, COLORPTR2PARAM2(ID2COLORPTR(TK_GREEN), 0.65));
    SDL_RenderDrawRect(renderer, &(SDL_Rect){POS(health_bar_lefttop), 43, 5});
    life_percentage = ((tk_float32_t)(tank->health) / tank->max_health);
    if (life_percentage >= 0.3) {
        SDL_SetRenderDrawColor(renderer, COLORPTR2PARAM2(ID2COLORPTR(TK_GREEN), 1));
    } else {
        SDL_SetRenderDrawColor(renderer, COLORPTR2PARAM2(ID2COLORPTR(TK_RED), 1));
    }
    SDL_RenderFillRect(renderer, &(SDL_Rect){POS(health_bar_lefttop), 
        (int)(life_percentage * 43), 5});

    if (TANK_ROLE_SELF == tank->role) {
        draw_tank_coordinates(renderer, tank);
    }
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

// 比较函数，用于qsort排序
static int compare_floats(const void *a, const void *b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

// 填充多边形函数 - 使用扫描线算法实现（points：多边形顶点数组，count：顶点数量）
int SDL_RenderFillPolygon(SDL_Renderer *renderer, const SDL_Point *points, int count) {
    /*test:
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_Point triangle[] = {
        {100, 100},
        {150, 50},
        {150, 150},
        {200, 200},
        {50, 200}
    };
    SDL_RenderFillPolygon(renderer, triangle, 5);
    */
    float intersections[count]; // 存储扫描线与形状的交点的x坐标值
    if (!renderer || !points || count < 3) return -1;
    memset(intersections, 0, sizeof(float)*count);

    // 找出多边形的最小和最大y坐标
    int min_y = points[0].y;
    int max_y = points[0].y;
    for (int i = 1; i < count; i++) {
        if (points[i].y < min_y) min_y = points[i].y;
        if (points[i].y > max_y) max_y = points[i].y;
    }

    // 为每条扫描线计算与多边形边的交点
    for (int y = min_y; y <= max_y; y++) {
        // 动态分配交点数组（最大可能交点数为顶点数）
        // float *intersections = (float*)malloc(count * sizeof(float));
        if (!intersections) return -1;

        int intersection_count = 0;
        // 遍历每条边
        for (int i = 0; i < count; i++) {
            const SDL_Point *p1 = &points[i];
            const SDL_Point *p2 = &points[(i + 1) % count];

            // 如果边平行于扫描线则跳过
            if (p1->y == p2->y) continue;
            // 确保p1的y坐标小于p2的y坐标
            if (p1->y > p2->y) {
                const SDL_Point *temp = p1;
                p1 = p2;
                p2 = temp;
            }
            // 如果扫描线在边的范围外则跳过
            if (y < p1->y || y > p2->y) continue;
            // 计算交点的x坐标（使用线性插值）
            float t = (float)(y - p1->y) / (p2->y - p1->y);
            float x = p1->x + t * (p2->x - p1->x);

            // 添加交点到数组
            intersections[intersection_count++] = x;
        }

        // 对交点按x坐标排序
        qsort(intersections, intersection_count, sizeof(float), compare_floats);

        // 填充每对交点之间的线段
        for (int i = 0; i < intersection_count - 1; i += 1) {
            int x1 = (int)ceilf(intersections[i]);
            int x2 = (int)floorf(intersections[i + 1]);

            // 绘制水平线段
            if (x1 <= x2) {
                SDL_RenderDrawLine(renderer, x1, y, x2, y);
            }
        }
        // free(intersections);
    }
    return 0;
}

#if 0
// 爆炸粒子渲染（极简版）
void render_explode_effect(SDL_Renderer* renderer, Tank *tank) {
    tk_uint8_t i = 0;
    ExplodeParticle* p = NULL;
    tk_float32_t size = 0;
    Rectangle rect;
    SDL_Color color;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < tank->explode_effect.active_count; i++) {
        p = &(tank->explode_effect.particles[i]);

        // 定义爆炸碎片形状（示例：绘制旋转的矩形）
        size = 10 * p->scale; // 大小随缩放变化
        calculate_rotated_rectangle(&p->position, size, size, p->angle, &rect);
        // 设置颜色（红色渐变，带透明度）
        color = (SDL_Color){255, 50, 50, p->alpha};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        // 绘制四个顶点形成碎片
        SDL_RenderDrawLine(renderer, 
            rect.lefttop.x, rect.lefttop.y, 
            rect.rightbottom.x, rect.rightbottom.y);
        SDL_RenderDrawLine(renderer, 
            rect.righttop.x, rect.righttop.y, 
            rect.leftbottom.x, rect.leftbottom.y);
    }
}
#else
void render_explode_effect(SDL_Renderer* renderer, Tank *tank) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    for (int i = 0; i < tank->explode_effect.active_count; i++) {
        ExplodeParticle* p = &(tank->explode_effect.particles[i]);
        const int vertex_count = p->shape_type == 0 ? 3 : (p->shape_type == 1 ? 4 : 5);
        const Point* vertices = particle_shape_vertices[p->shape_type];

        // 计算变换后的顶点坐标
        Point transformed[5]; // 最多5个顶点
        for (int v = 0; v < vertex_count; v++) {
            // 缩放顶点
            Point scaled = {vertices[v].x * p->scale, vertices[v].y * p->scale};
            // 旋转顶点（绕中心点）
            transformed[v] = rotate_point(&scaled, p->angle, &(Point){0, 0});
            // 平移到粒子位置
            transformed[v].x += p->position.x;
            transformed[v].y += p->position.y;
        }

        // 设置颜色(沿用坦克自身颜色)
        // Uint8 r, g, b, a;
        // SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a); // 获取当前绘制颜色
        SDL_SetRenderDrawColor(renderer, tank->basic_color->r, tank->basic_color->g, tank->basic_color->b, p->alpha);

        // 绘制多边形
        SDL_Point sdl_points[5];
        for (int v = 0; v < vertex_count; v++) {
            sdl_points[v] = (SDL_Point){(int)transformed[v].x, (int)transformed[v].y};
        }
#if 1 // 颜色填充版
        SDL_RenderFillPolygon(renderer, sdl_points, vertex_count);
#else // 非填充版
        for (int v = 0; v < vertex_count; v++) {
            int next_v = (v + 1) % vertex_count;
            SDL_RenderDrawLine(renderer, sdl_points[v].x, sdl_points[v].y, 
                sdl_points[next_v].x, sdl_points[next_v].y);
        }
#endif
    }
}
#endif

// 触发爆炸（初始化爆炸粒子集合）
void trigger_explode(Tank *tank) {
    tk_uint8_t i = 0;
    ExplodeParticle* p = NULL;

    for (i = 0; i < MAX_PARTICLES; i++) {
        if (tank->explode_effect.active_count >= MAX_PARTICLES) {
            break; // 即使短期内重复trigger触发多次爆炸，只要上一个爆炸还没结束，就不会再次引起爆炸导致冲突：
            // 前提是设置为p->life = PARTICLE_MAX_LIFE; 当然正常只应触发一次爆炸
        }

        p = &(tank->explode_effect.particles[tank->explode_effect.active_count]);
        p->position = tank->position;

        // 随机速度（范围：-3到+3）
        p->velocity.x = (rand() % 600 - 300) / 100.0f;
        p->velocity.y = (rand() % 600 - 300) / 100.0f;
        // 随机旋转角度
        p->angle = rand() % 360;
        // 初始缩放和透明度
        p->scale = (rand() % 50 + 50) / 100.0f; // 0.5~1.0
        p->alpha = 255;
        // p->life = PARTICLE_MAX_LIFE;
        p->life = rand() % (PARTICLE_MAX_LIFE-25) + 25; // 生命周期随机（25~PARTICLE_MAX_LIFE帧）
        p->shape_type = rand() % 3; // 随机形状

        tank->explode_effect.active_count++;
    }
}

// 更新爆炸粒子集合
void update_explode_particles_state(Tank *tank) {
    tk_uint8_t i = 0;
    ExplodeParticle* p = NULL;

    // 更新爆炸粒子
    for (i = 0; i < tank->explode_effect.active_count; i++) {
        p = &(tank->explode_effect.particles[i]);

        // 移动粒子
        p->position.x += p->velocity.x;
        p->position.y += p->velocity.y;
        // 衰减速度（模拟空气阻力）
        p->velocity.x *= 0.95f;
        p->velocity.y *= 0.95f;
        // 衰减透明度和生命周期
        p->scale += 0.03f; // 碎片逐渐变大
        // p->alpha = (uint8_t)(255 * (p->life / (float)PARTICLE_MAX_LIFE));
        p->alpha = (uint8_t)(255 * pow(p->life / (float)PARTICLE_MAX_LIFE, 2)); // 二次方衰减透明度
        p->life--;

        // 如果生命周期结束，标记为无效，并将粒子交换到数组末尾并减少计数
        if (p->life <= 0) {
            tank->explode_effect.active_count--;
            tank->explode_effect.particles[i] = \
                tank->explode_effect.particles[tank->explode_effect.active_count];
            i--; // 避免跳过下一个元素
        }
    }
}

// 渲染场景
static void render_gui_scene(Tank* tank) {
    // 清空屏幕
    SDL_SetRenderDrawColor(tk_renderer, COLOR2PARAM(ID2COLOR(TK_WHITE)));
    SDL_RenderClear(tk_renderer);

    // 渲染坦克
    render_tank(tk_renderer, tank);

    // 显示渲染内容
    SDL_RenderPresent(tk_renderer);
}

Tank* create_tank(tk_uint8_t *name, Point pos, tk_float32_t angle_deg, tk_uint8_t role) {
    Tank *tank = NULL;

    tank = malloc(sizeof(Tank));
    if (!tank) {
        goto error;
    }
    memset(tank, 0, sizeof(Tank));

    strlcpy(tank->name, name, sizeof(tank->name));
    tank->id = id_pool_allocate(tk_idpool);
    if (!tank->id) {
        printf("Error: id_pool_allocate failed\n");
        goto error;
    }
    tank->position = pos;
    tank->angle_deg = angle_deg;
    tank->role = role;
    SET_FLAG(tank, flags, TANK_ALIVE);
    tank->basic_color = (TANK_ROLE_SELF == tank->role) ? ID2COLORPTR(TK_BLUE) : ID2COLORPTR(TK_RED);
    tank->health = tank->max_health = (TANK_ROLE_SELF == tank->role) ? 50 : 250;
    tank->speed = TANK_INIT_SPEED;

    printf("create a tank(name:%s, id:%lu, total size:%luB, ExplodeEffect's size: %luB) success\n", 
        tank->name, tank->id, sizeof(Tank), sizeof(tank->explode_effect));
    return tank;
error:
    printf("Error: create tank %s failed\n", name);
    if (tank) {
        free(tank);
    }
    return NULL;
}

void delete_tank(Tank **tank) {
    if (!tank || !(*tank)) {
        if (!tank) {
            *tank = NULL;
        }
        return;
    }
    printf("tank(id:%lu) %s(flags:%lu) is deleted\n", (*tank)->id, (*tank)->name, (*tank)->flags);
    free(*tank);
    *tank = NULL;
}

#if 1
int main() { //gcc tank.c -o tank `sdl2-config --cflags --libs` -lm -g -lbsd
    int quit = 0;
    SDL_Event e;
    tk_float32_t new_dir = 0;
    Tank *tank = NULL;
    int ret = -1;

    // 结合时间和进程ID作为随机种子（用于爆炸粒子）
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    if (init_gui() != 0){
        goto out;
    }
    tk_idpool = id_pool_create(ID_POOL_SIZE);
    if (!tk_idpool) {
        goto out;
    }
    tank = create_tank("muggledy", (Point){400,300}, 300, TANK_ROLE_SELF);
    if (!tank) {
        goto out;
    }
    // 主循环
    while (!quit) {
        // 处理事件
        while (SDL_PollEvent(&e) != 0) {
            // 用户请求退出
            if (e.type == SDL_QUIT) {
                quit = 1;
            } else if (e.type == SDL_KEYDOWN) { // 处理键盘事件
                if (tank->health > 0) {
                    tank->health--;
                } else {
                    break;
                }
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = 1;
                        break;
                    case SDLK_w:
                        tank->position = move_point(tank->position, tank->angle_deg, tank->speed);
                        break;
                    case SDLK_s:
                        new_dir = tank->angle_deg + 180;
                        if (new_dir > 360) {
                            new_dir -= 360;
                        }
                        tank->position = move_point(tank->position, new_dir, tank->speed);
                        break;
                    case SDLK_a:
                        tank->angle_deg += 360;
                        tank->angle_deg -= 10;
                        if (tank->angle_deg >= 360) {
                            tank->angle_deg -= 360;
                        }
                        break;
                    case SDLK_d:
                        tank->angle_deg += 10;
			            if (tank->angle_deg >= 360) {
                            tank->angle_deg -= 360;
                        }
                        break;
                }
            }
        }

        // 渲染场景
        render_gui_scene(tank);
        // 控制帧率
        SDL_Delay(30); // 约60FPS
    }
    ret = 0;

out:
    delete_tank(&tank);
    if (tk_idpool) {
        id_pool_destroy(tk_idpool);
        tk_idpool = NULL;
    }
    cleanup_gui();
    return ret;
}
#endif