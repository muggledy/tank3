#include <math.h>
#include "gui_tank.h"
#include <bsd/string.h>
#include "tools.h"
#include "debug.h"
#include "event_loop.h"

extern MazePathBFSearchManager tk_bfs_search_manager;

// SDL相关变量
SDL_Window*   tk_window = NULL;
SDL_Renderer* tk_renderer = NULL;
TTF_Font* tank_font8 = NULL;
KeyValue tk_key_value;
TankMusic tk_music;
tk_uint8_t tk_gui_stop_game = 0;
Button* tk_stop_game_button = NULL;

// 颜色数组
SDL_Color tk_colors[] = {
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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        tk_debug("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    // 创建窗口
    tk_window = SDL_CreateWindow("坦克3", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                             800, 600, SDL_WINDOW_SHOWN);
    if (tk_window == NULL) {
        tk_debug("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -2;
    }

    // 创建渲染器
    tk_renderer = SDL_CreateRenderer(tk_window, -1, SDL_RENDERER_ACCELERATED);
    if (tk_renderer == NULL) {
        tk_debug("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
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

int load_music(MusicEntry *music, char *file_path) {
    music->channel = -1;
    music->sound = Mix_LoadWAV(file_path);
    if(music->sound == NULL) {
        tk_debug("无法加载声音(%s): %s\n", file_path, Mix_GetError());
        return -1;
    }
    return 0;
}

int init_music() { // call after init_gui()
    // 初始化SDL_mixer
    if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        tk_debug("SDL_mixer初始化失败: %s\n", Mix_GetError());
        return -1;
    }
    memset(&tk_music, 0, sizeof(tk_music));

    // 加载坦克声音
    if (load_music(&(tk_music.move), DEFAULT_TANK_MOVE_MUSIC_PATH) != 0) {
        return -1;
    }
    if (load_music(&(tk_music.explode), DEFAULT_TANK_EXPLODE_MUSIC_PATH) != 0) {
        return -1;
    }
    if (load_music(&(tk_music.shoot), DEFAULT_TANK_SHOOT_MUSIC_PATH) != 0) {
        return -1;
    }
    if (load_music(&(tk_music.hit), DEFAULT_TANK_HIT_MUSIC_PATH) != 0) {
        return -1;
    }

    return 0;
}

extern int is_music_playing(MusicEntry *music);
extern void pause_music(MusicEntry *music);

void pause_and_free_music(MusicEntry *music) {
    if (!music->sound) {
        music->channel = -1;
        return;
    }
    if (is_music_playing(music)) {
        pause_music(music);
    }
    Mix_FreeChunk(music->sound);
    music->sound = NULL;
    music->channel = -1;
}

void cleanup_music() { // call before cleanup_gui()
    // 释放声音资源
    pause_and_free_music(&(tk_music.move));
    pause_and_free_music(&(tk_music.explode));
    pause_and_free_music(&(tk_music.shoot));
    pause_and_free_music(&(tk_music.hit));

    // 关闭SDL_mixer
    Mix_CloseAudio();
    Mix_Quit();
}

int is_music_playing(MusicEntry *music) {
    if(music->channel != -1) {
        return 1;
    }
    return 0;
}

int play_music(MusicEntry *music, int only_once) {
    if(music->channel != -1) { // is playing
        return 0;
    }
    music->channel = Mix_PlayChannel(-1, music->sound, only_once ? 0 : -1);
    if(music->channel == -1) {
        tk_debug("无法播放声音: %s\n", Mix_GetError());
        return -1;
    }
    return 0;
}

void pause_music(MusicEntry *music) {
    if(music->channel != -1) {
        Mix_HaltChannel(music->channel);
        music->channel = -1;
    }
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

Rectangle draw_solid_rectangle(SDL_Renderer* renderer, const Point *center, 
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
static void draw_tank_coordinates(SDL_Renderer* renderer, Tank* tank) {
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
#if 0
    SDL_RenderDrawLine(renderer, origin.x, origin.y, tank_angle_end.x, tank_angle_end.y);
#else
    // 绘制坦克前进方向轴（遇墙壁自动反射版本）
    Ray_Intersection_Dot_Info info;
    info.start_point = origin;
    info.angle_deg = tank->angle_deg;
    info.current_grid = get_grid_by_tank_position(&tank->position);
    info.terminate_flag = 0;
    tk_debug_internal(DEBUG_SIGHT_LINE, "draw_tank_coordinates...\n");
    for (int i=0; i<6; i++) { // 至多反射6次
        get_ray_intersection_dot_with_grid(&info);
        if (info.terminate_flag) {
            break;
        }
        SDL_RenderDrawLine(renderer, POS(info.start_point), POS(info.intersection_dot));
        info.start_point = info.intersection_dot;
        if ((info.current_grid.x == info.next_grid.x) && (info.current_grid.y == info.next_grid.y)) {
            info.angle_deg = info.reflect_angle_deg;
        }
        info.current_grid = info.next_grid;
    }
#endif

#if 1 // 绘制前进方向的角度文本值
    Point text_pos = get_line_k_center(&origin, &tank_angle_end, 0.2);
    if (!tank_font8) {
        tank_font8 = load_cached_font(DEFAULT_FONT_PATH, 8);
    }
    SDL_Texture* text1 = render_cached_text(renderer, tank_font8, uint_to_str(tank->angle_deg), ID2COLOR(TK_BLACK)); // TODO: 缓存只有100，
    // 而旋转角度有360，因此缓存经常失效，而且当前缓存是基于数组实现，性能较差，应该改造为基于哈希表
    draw_text(renderer, text1, POS(text_pos));
#endif
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
static void render_explode_effect(SDL_Renderer* renderer, Tank *tank) {
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
static void render_explode_effect(SDL_Renderer* renderer, Tank *tank) {
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
        SDL_SetRenderDrawColor(renderer, TANKCOLORPTR(tank)->r, TANKCOLORPTR(tank)->g, TANKCOLORPTR(tank)->b, p->alpha);

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
static void trigger_explode(Tank *tank) {
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
static void update_explode_particles_state(Tank *tank) {
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

void draw_rectangle(SDL_Renderer* renderer, Rectangle *rect) {
    if (!renderer || !rect) return;
    SDL_RenderDrawLine(renderer, POS(rect->lefttop), POS(rect->righttop));
    SDL_RenderDrawLine(renderer, POS(rect->righttop), POS(rect->rightbottom));
    SDL_RenderDrawLine(renderer, POS(rect->rightbottom), POS(rect->leftbottom));
    SDL_RenderDrawLine(renderer, POS(rect->leftbottom), POS(rect->lefttop));
}

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
        if (TST_FLAG(tank, flags, TANK_ALIVE)) { // only enter once
            CLR_FLAG(tank, flags, TANK_ALIVE);
            trigger_explode(tank);
            SET_FLAG(tank, flags, TANK_DYING);
            if (is_music_playing(&(tk_music.explode))) {
                pause_music(&(tk_music.explode));
            }
            play_music(&(tk_music.explode), 1);
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
    SDL_Color *color = TANKCOLORPTR(tank);
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
    rect = draw_solid_rectangle(renderer, &(tank->position), TANK_LENGTH, TANK_WIDTH, angle_deg, &body_color);

    // 绘制履带
    topline_center = get_line_center(&rect.lefttop, &rect.righttop);
    bottomline_center = get_line_center(&rect.leftbottom, &rect.rightbottom);
    draw_solid_rectangle(renderer, &topline_center, TANK_LENGTH-4, 4, angle_deg, color);
    draw_solid_rectangle(renderer, &bottomline_center, TANK_LENGTH-4, 4, angle_deg, color);

    // 绘制炮塔
    draw_solid_rectangle(renderer, &(tank->position), 15, 15, angle_deg, color);
    rightline_center = get_line_center(&rect.righttop, &rect.rightbottom);
    gun_barrel_center = get_line_k_center(&(tank->position), &rightline_center, 0.8);
    draw_solid_rectangle(renderer, &gun_barrel_center, 18, 9, angle_deg, color);

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

    // 绘制坦克名称
    tank_font8 = load_cached_font(DEFAULT_FONT_PATH, 8); // TODO: 改造为哈希表实现
    SDL_Texture* text1 = render_cached_text(renderer, tank_font8, tank->name, ID2COLOR(TK_BLACK));
    draw_text(renderer, text1, tank->position.x-22, tank->position.y-43);

    if (TANK_ROLE_SELF == tank->role) {
        draw_tank_coordinates(renderer, tank);
    }
    tank_font8 = NULL;

    // 绘制坦克外轮廓边界（仅用于测试碰撞检测功能）
#if 0
    SDL_SetRenderDrawColor(renderer, COLORPTR2PARAM2(ID2COLORPTR(TK_BLACK), 1));
    draw_rectangle(renderer, &tank->outline);
#endif
}

// 绘制碰撞预警特效
void draw_collision_warning(SDL_Renderer* renderer, Tank* tank) {
    if (!renderer || !tank) return;
    if ((tank->collision_flag << 4) == 0) return;
    if (!TST_FLAG(tank, flags, TANK_ALIVE)) return;

    // 设置预警颜色（半透明红色，TODO: 碰撞强度影响透明度）
    Uint8 alpha = (Uint8)(255 * 0.5);
    Point p;
    SDL_Color color = {255, 0, 0, alpha};

    // 根据碰撞方向绘制相应的预警区域
    if (TST_FLAG(tank, collision_flag, COLLISION_FRONT)) {
        p = get_line_center(&tank->outline.righttop, &tank->outline.rightbottom);
        draw_solid_rectangle(renderer, &p, 30, 5, tank->angle_deg, &color);
    }
    if (TST_FLAG(tank, collision_flag, COLLISION_BACK)) {
        p = get_line_center(&tank->outline.lefttop, &tank->outline.leftbottom);
        draw_solid_rectangle(renderer, &p, 30, 5, tank->angle_deg, &color);
    }
    if (TST_FLAG(tank, collision_flag, COLLISION_LEFT)) {
        p = get_line_center(&tank->outline.lefttop, &tank->outline.righttop);
        draw_solid_rectangle(renderer, &p, 30, 5, tank->angle_deg+90, &color);
    }
    if (TST_FLAG(tank, collision_flag, COLLISION_RIGHT)) {
        p = get_line_center(&tank->outline.rightbottom, &tank->outline.leftbottom);
        draw_solid_rectangle(renderer, &p, 30, 5, tank->angle_deg+90, &color);
    }
}

void draw_solid_circle(SDL_Renderer* renderer, tk_float32_t x0, tk_float32_t y0, tk_float32_t r, SDL_Color *color) {
    SDL_SetRenderDrawColor(renderer, COLORPTR2PARAM(color));
    
    for (int y = -r; y <= r; y++) {
        int dy = y * y;
        int max_x = (int)sqrt(r*r - dy);
        for (int x = -max_x; x <= max_x; x++) {
            SDL_RenderDrawPoint(renderer, x0 + x, y0 + y);
        }
    }
}

// 绘制炮弹
void draw_shell(SDL_Renderer* renderer, Shell *shell) {
    if (!shell->ttl) return;
    draw_solid_circle(renderer, POS(shell->position), SHELL_RADIUS_LENGTH, (SDL_Color*)(((Tank*)(shell->tank_owner))->basic_color));
}

// 对目标位置pos1进行偏移处理（pos2为偏移量）
#define POS_OFFSET(pos1, pos2) (pos1).x+(pos2).x,(pos1).y+(pos2).y

SDL_Color search_path_grid_color = {255, 182, 193, 50};  // RGBA格式

// 判断next网格在current网格的上下左右方位：1、2、3、4（前提需保证，它俩上下左右相邻）
int get_relation_of_two_grids(Grid *current, Grid *next) {
    if (((current->x < 0) || (current->y < 0)) || ((next->x < 0) || (next->y < 0))) {
        return 0;
    }
    if (next->x == current->x) {
        if (next->y < current->y) {
            return 1;
        } else if (next->y > current->y) {
            return 2;
        }
    } else if (next->y == current->y) {
        if (next->x < current->x) {
            return 3;
        } else if (next->x > current->x) {
            return 4;
        }
    }
    return 0;
}

void fill_grid(SDL_Renderer* renderer, Grid *previous, Grid *current, Grid *next) {
    int relation1 = get_relation_of_two_grids(current, previous);
    int relation2 = get_relation_of_two_grids(current, next);
    SDL_SetRenderDrawColor(renderer, COLOR2PARAM(search_path_grid_color));
    SDL_Rect rect = {current->x * GRID_SIZE + tk_maze_offset.x + 1, current->y * GRID_SIZE + tk_maze_offset.y + 1, GRID_SIZE-2, GRID_SIZE-2};
    if (relation1 == 1) {
        rect.y -= 2;
        rect.h += 2;
    } else if (relation1 == 2) {
        rect.h += 2;
    } else if (relation1 == 3) {
        rect.x -= 2;
        rect.w += 2;
    } else if (relation1 == 4) {
        rect.w += 2;
    }
    SDL_RenderFillRect(renderer, &rect);
}

// 渲染场景
void render_gui_scene() {
    // tk_debug("render_gui_scene...\n");
    Tank *tank = NULL;
    Shell *shell = NULL;
    Grid previous = {-1, -1}, current, next;

    // 清空屏幕
    SDL_SetRenderDrawColor(tk_renderer, COLOR2PARAM(ID2COLOR(TK_WHITE)));
    SDL_RenderClear(tk_renderer);

    // 绘制墙壁
    SDL_SetRenderDrawColor(tk_renderer, COLOR2PARAM(ID2COLOR(TK_BLACK)));
    for (int i=0; i<tk_shared_game_state.blocks_num; i++) {
        SDL_RenderDrawLine(tk_renderer, POS_OFFSET(tk_shared_game_state.blocks[i].start, tk_maze_offset), 
            POS_OFFSET(tk_shared_game_state.blocks[i].end, tk_maze_offset));
    }

    // 绘制搜索路径
    SDL_SetRenderDrawBlendMode(tk_renderer, SDL_BLENDMODE_BLEND); // 启用混合
    lock(&tk_bfs_search_manager.spinlock);
    if (tk_bfs_search_manager.success) {
        FOREACH_BFS_SEARCH_MANAGER_GRID(&tk_bfs_search_manager, current) {
            next = NEXT_BFS_SEARCH_GRID(&tk_bfs_search_manager, current);
            fill_grid(tk_renderer, &previous, &current, &next);
            previous = current;
        }
    }
    unlock(&tk_bfs_search_manager.spinlock);

    // 渲染坦克和炮弹
    lock(&tk_shared_game_state.spinlock);
    TAILQ_FOREACH(tank, &tk_shared_game_state.tank_list, chain) {
        if (TST_FLAG(tank, flags, TANK_IS_HIT_BY_ENEMY)) {
            CLR_FLAG(tank, flags, TANK_IS_HIT_BY_ENEMY);
            if (is_music_playing(&(tk_music.hit))) {
                pause_music(&(tk_music.hit));
            }
            play_music(&(tk_music.hit), 1); // 播放坦克被击中的音效
        }
        draw_tank(tk_renderer, tank);
        draw_collision_warning(tk_renderer, tank);
        lock(&tank->spinlock); // 尽管GUI线程存在着两把锁的“请求和保持”问题，但控制线程没有该问题，因此无死锁
        TAILQ_FOREACH(shell, &tank->shell_list, chain) {
            draw_shell(tk_renderer, shell);
        }
        unlock(&tank->spinlock);
    }
    unlock(&tk_shared_game_state.spinlock);

    // 绘制按钮
    render_all_buttons(tk_renderer);

    // 显示渲染内容
    SDL_RenderPresent(tk_renderer);
}

int check_resource_file() {
    if (!get_absolute_path(DEFAULT_FONT_PATH)) {
        return -1;
    }
    if (!get_absolute_path(DEFAULT_TANK_MOVE_MUSIC_PATH)) {
        return -1;
    }
    if (!get_absolute_path(DEFAULT_TANK_EXPLODE_MUSIC_PATH)) {
        return -1;
    }
    if (!get_absolute_path(DEFAULT_TANK_SHOOT_MUSIC_PATH)) {
        return -1;
    }

    return 0;
}

void gui_init_tank(Tank *tank) {
    if (!tank) return;
    // 设置坦克颜色（玩家坦克为蓝色，AI坦克为红色）
    tank->basic_color = (void *)((TANK_ROLE_SELF == tank->role) ? ID2COLORPTR(TK_BLUE) : ID2COLORPTR(TK_RED));
}

void gui_init_all_tank() {
    Tank *tank = NULL;
    lock(&tk_shared_game_state.spinlock);
    TAILQ_FOREACH(tank, &tk_shared_game_state.tank_list, chain) {
        gui_init_tank(tank);
    }
    unlock(&tk_shared_game_state.spinlock);
}

void notify_control_thread_exit() {
    Event *e = NULL;
    e = create_event(EVENT_QUIT);
    if (!e) {
        exit(1);
    }
    enqueue_event(&tk_event_queue, e);
    notify_event_loop();
    close_write_end_of_pipe();
}

void notify_control_thread_stop() {
    Event *e = NULL;
    e = create_event(EVENT_GAME_STOP);
    if (!e) {
        exit(1);
    }
    enqueue_event(&tk_event_queue, e);
    notify_event_loop();
}

void notify_control_thread_start() {
    Event *e = NULL;
    e = create_event(EVENT_GAME_START);
    if (!e) {
        exit(1);
    }
    enqueue_event(&tk_event_queue, e);
    notify_event_loop();
}

void send_key_to_control_thread(int key_type, int key_value) {
    Event *e = NULL;
    int type = key_type;
    int value = key_value;

#if 0
    switch (key_type) {
        case SDL_KEYDOWN:
            type = EVENT_KEY_PRESS;
            break;
        case SDL_KEYUP:
            type = EVENT_KEY_RELEASE;
            break;
        default:
            return;
    }
    switch (key_value) {
        case SDLK_w:
            value = KEY_W;
            break;
        case SDLK_s:
            value = KEY_S;
            break;
        case SDLK_a:
            value = KEY_A;
            break;
        case SDLK_d:
            value = KEY_D;
            break;
        default:
            return;
    }
#endif
    e = create_event(type);
    if (!e) {
        exit(1);
    }
    e->data.key = value;
    enqueue_event(&tk_event_queue, e);
    notify_event_loop();
}

#define OP_LIST_LEN 50
static int op_cursor = OP_LIST_LEN-1;
#define OP_NULL     0
#define OP_K_W_DOWN 1
#define OP_K_S_DOWN 2
#define OP_K_A_DOWN 3
#define OP_K_D_DOWN 4
#define OP_K_W_UP   5
#define OP_K_S_UP   6
#define OP_K_A_UP   7
#define OP_K_D_UP   8
#define OP_K_SPACE_DOWN 9
static int op_list[OP_LIST_LEN];

static void insert_op_list(int op) {
    if (op == op_list[op_cursor]) return;
    op_list[op_cursor] = op;
    op_cursor--;
    if (op_cursor < -1) {
        tk_debug("Error: insert_op_list failed for overflow(%d)\n", op_cursor);
        exit(1);
    }
}

static void init_op_list() {
    op_cursor = OP_LIST_LEN-1;
    op_list[op_cursor] = 0;
}

#define iter_op_list(op) for(int _i=OP_LIST_LEN-1, op=op_list[_i]; _i>op_cursor; _i--, op=op_list[_i])

static void print_op_list() {
    int op = 0;
    tk_debug("op_list: ");
    iter_op_list(op) {
        printf("%d, ", op);
    }
    printf("\n");
}

static int get_op_list_num() {
    return (OP_LIST_LEN - op_cursor - 1);
}

int get_grid_by_key_mouse(int mouseX, int mouseY, Grid *grid) {
    Point p = {mouseX, mouseY};
    if (((mouseX > tk_maze_offset.x) && (mouseX < (HORIZON_GRID_NUMBER*GRID_SIZE+tk_maze_offset.x))) 
        && ((mouseY > tk_maze_offset.y) && (mouseY < (VERTICAL_GRID_NUMBER*GRID_SIZE+tk_maze_offset.y)))) {
        *grid = get_grid_by_tank_position(&p);
        return 0;
    } else {
        return -1;
    }
}

void send_maze_path_search_request_to_control_thread(Grid *end) {
    Event *e = NULL;
    e = create_event(EVENT_PATH_SEARCH);
    if (!e) {
        exit(1);
    }
    e->data.path_search_request.end = *end;
    enqueue_event(&tk_event_queue, e);
    notify_event_loop();
}

void handle_click_event_for_all_grids(SDL_Event* event) {
    if (event->type != SDL_MOUSEBUTTONDOWN) return;
    int mouseX, mouseY;
    Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
    Grid grid;

    if (get_grid_by_key_mouse(mouseX, mouseY, &grid) == 0) {
        tk_debug_internal(DEBUG_GUI_THREAD_DETAIL, "网格(%d,%d)被点击\n", POS(grid));
        send_maze_path_search_request_to_control_thread(&grid);
    }
}

void gui_main_loop() {
    int quit = 0;
    SDL_Event e;
    int op = 0;
    while (!quit) {
        // 处理事件
        init_op_list();
        while (SDL_PollEvent(&e) != 0) {
            // 用户请求退出
            if (e.type == SDL_QUIT) {
                quit = 1;
                // stop_event_loop();
                notify_control_thread_exit();
                goto out;
            } else if (e.type == SDL_KEYDOWN) { // 处理键盘事件
                if (tk_gui_stop_game && (e.key.keysym.sym != SDLK_ESCAPE)) {
                    break;
                }
                // printf(">>> key %d down\n", e.key.keysym.sym);
                // if (mytankptr && (mytankptr->health > 0)) {
                //     mytankptr->health--;
                // } else {
                //     break;
                // }
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = 1;
                        // stop_event_loop();
                        notify_control_thread_exit();
                        goto out;
                        // break;
                    case SDLK_w:
                        // send_key_to_control_thread(EVENT_KEY_PRESS, KEY_W);
                        insert_op_list(OP_K_W_DOWN); // op_list用于实现按键去重
                        PLAY_MOVE_MUSIC();
                        SET_FLAG(&tk_key_value, mask, TK_KEY_W_ACTIVE);
                        break;
                    case SDLK_s:
                        // send_key_to_control_thread(EVENT_KEY_PRESS, KEY_S);
                        insert_op_list(OP_K_S_DOWN);
                        PLAY_MOVE_MUSIC();
                        SET_FLAG(&tk_key_value, mask, TK_KEY_S_ACTIVE);
                        break;
                    case SDLK_a:
                        // send_key_to_control_thread(EVENT_KEY_PRESS, KEY_A);
                        insert_op_list(OP_K_A_DOWN);
                        PLAY_MOVE_MUSIC();
                        SET_FLAG(&tk_key_value, mask, TK_KEY_A_ACTIVE);
                        break;
                    case SDLK_d:
                        // send_key_to_control_thread(EVENT_KEY_PRESS, KEY_D);
                        insert_op_list(OP_K_D_DOWN);
                        PLAY_MOVE_MUSIC();
                        SET_FLAG(&tk_key_value, mask, TK_KEY_D_ACTIVE);
                        break;
                    case SDLK_SPACE: // 发射子弹
                        if (mytankptr && !TST_FLAG(mytankptr, flags, TANK_FORBID_SHOOT)) {
                            if (is_music_playing(&(tk_music.shoot))) {
                                pause_music(&(tk_music.shoot));
                            }
                            play_music(&(tk_music.shoot), 1);
                        }
                        insert_op_list(OP_K_SPACE_DOWN);
                        break;
                }
            } else if (e.type == SDL_KEYUP) {
                if (tk_gui_stop_game) {
                    break;
                }
                switch (e.key.keysym.sym) {
                    case SDLK_w:
                        // send_key_to_control_thread(EVENT_KEY_RELEASE, KEY_W);
                        insert_op_list(OP_K_W_UP);
                        CLR_FLAG(&tk_key_value, mask, TK_KEY_W_ACTIVE);
                        PAUSE_MOVE_MUSIC();
                        break;
                    case SDLK_s:
                        // send_key_to_control_thread(EVENT_KEY_RELEASE, KEY_S);
                        insert_op_list(OP_K_S_UP);
                        CLR_FLAG(&tk_key_value, mask, TK_KEY_S_ACTIVE);
                        PAUSE_MOVE_MUSIC();
                        break;
                    case SDLK_a:
                        // send_key_to_control_thread(EVENT_KEY_RELEASE, KEY_A);
                        insert_op_list(OP_K_A_UP);
                        CLR_FLAG(&tk_key_value, mask, TK_KEY_A_ACTIVE);
                        PAUSE_MOVE_MUSIC();
                        break;
                    case SDLK_d:
                        // send_key_to_control_thread(EVENT_KEY_RELEASE, KEY_D);
                        insert_op_list(OP_K_D_UP);
                        CLR_FLAG(&tk_key_value, mask, TK_KEY_D_ACTIVE);
                        PAUSE_MOVE_MUSIC();
                        break;
                }
            } else if ((e.type == SDL_MOUSEMOTION) || (e.type == SDL_MOUSEBUTTONDOWN) || (e.type == SDL_MOUSEBUTTONUP)) {
                handle_click_event_for_all_buttons(&e);
                handle_click_event_for_all_grids(&e);
            }
        }
        if (!tk_gui_stop_game) {
        // print_op_list();
        iter_op_list(op) {
            if (OP_K_W_DOWN == op) {
                send_key_to_control_thread(EVENT_KEY_PRESS, KEY_W);
            } else if (OP_K_S_DOWN == op) {
                send_key_to_control_thread(EVENT_KEY_PRESS, KEY_S);
            } else if (OP_K_A_DOWN == op) {
                send_key_to_control_thread(EVENT_KEY_PRESS, KEY_A);
            } else if (OP_K_D_DOWN == op) {
                send_key_to_control_thread(EVENT_KEY_PRESS, KEY_D);
            } else if (OP_K_W_UP == op) {
                send_key_to_control_thread(EVENT_KEY_RELEASE, KEY_W);
            } else if (OP_K_S_UP == op) {
                send_key_to_control_thread(EVENT_KEY_RELEASE, KEY_S);
            } else if (OP_K_A_UP == op) {
                send_key_to_control_thread(EVENT_KEY_RELEASE, KEY_A);
            } else if (OP_K_D_UP == op) {
                send_key_to_control_thread(EVENT_KEY_RELEASE, KEY_D);
            } else if (OP_K_SPACE_DOWN == op) {
                send_key_to_control_thread(EVENT_KEY_PRESS, KEY_SPACE);
            }
        }
        if ((get_op_list_num() == 0) && (tk_key_value.mask != 0) /*&& (tk_shared_game_state.game_time % 2 == 0) && 0*/) {
            tk_debug_internal(DEBUG_GUI_THREAD_DETAIL, "auto send key event(%u)\n", tk_shared_game_state.game_time);
            if (TST_FLAG(&tk_key_value, mask, TK_KEY_W_ACTIVE)) {
                send_key_to_control_thread(EVENT_KEY_PRESS, KEY_W);
            } else if (TST_FLAG(&tk_key_value, mask, TK_KEY_S_ACTIVE)) {
                send_key_to_control_thread(EVENT_KEY_PRESS, KEY_S);
            } else if (TST_FLAG(&tk_key_value, mask, TK_KEY_A_ACTIVE)) {
                send_key_to_control_thread(EVENT_KEY_PRESS, KEY_A);
            } else if (TST_FLAG(&tk_key_value, mask, TK_KEY_D_ACTIVE)) {
                send_key_to_control_thread(EVENT_KEY_PRESS, KEY_D);
            }
        }
        }
        // 渲染场景
        render_gui_scene();
        tk_shared_game_state.game_time++;
        // 控制帧率
        SDL_Delay(RENDER_FPS_MS); // 42ms约24FPS
    }
out:
    return;
}

int init_simple_game_tanks() {
    if (mytankptr) {
        delete_tank(mytankptr, 1);
    }
    create_tank("yangdai", get_random_grid_pos_for_tank(), 300, TANK_ROLE_SELF);
    create_tank("muggle-0", get_random_grid_pos_for_tank(), random_range(0, 360), TANK_ROLE_ENEMY_MUGGLE);
    if (!mytankptr) {
        return -1;
    }
    gui_init_all_tank();
    return 0;
}

#define BUTTON_GAME_RESTART 0x01
void stop_game_button_click_callback(void* button, void* data) {
    tk_debug("按钮[%s]被点击! \n", ((Button*)button)->text);
    if (!TST_FLAG(((Button*)button), user_flag, BUTTON_GAME_RESTART)) {
        SET_FLAG(((Button*)button), user_flag, BUTTON_GAME_RESTART);
        strlcpy(((Button*)button)->text, "开始", sizeof(((Button*)button)));
        tk_gui_stop_game = 1;
        notify_control_thread_stop();
    } else {
        CLR_FLAG(((Button*)button), user_flag, BUTTON_GAME_RESTART);
        strlcpy(((Button*)button)->text, "暂停", sizeof(((Button*)button)));
        tk_gui_stop_game = 0;
        notify_control_thread_start();
    }
}

void restart_game_button_click_callback(void* button, void* data) {
    tk_debug("按钮[%s]被点击! \n", ((Button*)button)->text);
    delete_all_tanks();
    if (tk_gui_stop_game || tk_shared_game_state.stop_game) {
        if (tk_stop_game_button) {
            CLR_FLAG(tk_stop_game_button, user_flag, BUTTON_GAME_RESTART);
            strlcpy(tk_stop_game_button->text, "暂停", sizeof(((Button*)button)));
        } else {
            exit(1);
        }
        notify_control_thread_start();
        tk_gui_stop_game = 0;
        tk_shared_game_state.stop_game = 0;
    }
    init_simple_game_tanks();
}

int init_game_buttons() {
    if ((tk_stop_game_button = create_button(tk_maze_offset.x+GRID_SIZE*HORIZON_GRID_NUMBER+10, tk_maze_offset.y, 50, 30, 8, 2, 
        "暂停", stop_game_button_click_callback, NULL)) == NULL) {
        return -1;
    }
    if (create_button(tk_maze_offset.x+GRID_SIZE*HORIZON_GRID_NUMBER+10, tk_maze_offset.y+40, 82, 30, 8, 2, 
        "重开一局", restart_game_button_click_callback, NULL) == NULL) {
        return -1;
    }
    return 0;
}

#if 0
int main() { //gcc tank.c -o tank `sdl2-config --cflags --libs` -lm -g -lbsd
    int quit = 0;
    SDL_Event e;
    Tank *tank = NULL;
    int ret = -1;

    // 确认游戏资源是否存在
    if (check_resource_file() != 0) {
        return ret;
    }

    // 结合时间和进程ID作为随机种子（用于爆炸粒子）
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    if (init_gui() != 0) {
        goto out;
    }
    if (init_music() != 0) {
        goto out;
    }
    if (init_ttf() != 0) {
        goto out;
    }
    if (init_idpool() != 0) {
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
                        PLAY_MOVE_MUSIC();
                        SET_FLAG(&tk_key_value, mask, TK_KEY_W_ACTIVE);
                        break;
                    case SDLK_s:
                        PLAY_MOVE_MUSIC();
                        SET_FLAG(&tk_key_value, mask, TK_KEY_S_ACTIVE);
                        break;
                    case SDLK_a:
                        PLAY_MOVE_MUSIC();
                        SET_FLAG(&tk_key_value, mask, TK_KEY_A_ACTIVE);
                        break;
                    case SDLK_d:
                        PLAY_MOVE_MUSIC();
                        SET_FLAG(&tk_key_value, mask, TK_KEY_D_ACTIVE);
                        break;
                }
            } else if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                    case SDLK_w:
                        CLR_FLAG(&tk_key_value, mask, TK_KEY_W_ACTIVE);
                        PAUSE_MOVE_MUSIC();
                        break;
                    case SDLK_s:
                        CLR_FLAG(&tk_key_value, mask, TK_KEY_S_ACTIVE);
                        PAUSE_MOVE_MUSIC();
                        break;
                    case SDLK_a:
                        CLR_FLAG(&tk_key_value, mask, TK_KEY_A_ACTIVE);
                        PAUSE_MOVE_MUSIC();
                        break;
                    case SDLK_d:
                        CLR_FLAG(&tk_key_value, mask, TK_KEY_D_ACTIVE);
                        PAUSE_MOVE_MUSIC();
                        break;
                }
            }
            if (((e.type == SDL_KEYDOWN) || (e.type == SDL_KEYUP)) && (tk_key_value.mask != 0)) {
                handle_key(tank);
            }
        }

        // 渲染场景
        render_gui_scene(tank);
        // 控制帧率
        SDL_Delay(30); // 约60FPS
    }
    ret = 0;
    print_text_cache();

out:
    delete_tank(&tank);
    cleanup_idpool();
    cleanup_ttf();
    cleanup_music();
    cleanup_gui();
    return ret;
}
#endif