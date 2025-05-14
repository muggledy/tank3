#include "gui.h"
#include "event_queue.h"
#include <SDL2/SDL.h>
#include <stdio.h>

// SDL相关变量
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static int should_quit = 0;

// 颜色定义
#define COLOR_WHITE {255, 255, 255, 255}
#define COLOR_BLACK {0, 0, 0, 255}
#define COLOR_RED {255, 0, 0, 255}
#define COLOR_GREEN {0, 255, 0, 255}
#define COLOR_BLUE {0, 0, 255, 255}
#define COLOR_YELLOW {255, 255, 0, 255}

// 初始化GUI
void init_gui(void) {
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return;
    }
    
    // 创建窗口
    window = SDL_CreateWindow("坦克游戏", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                             800, 600, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }
    
    // 创建渲染器
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return;
    }
    
    // 设置渲染器绘制颜色为白色
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

// 清理GUI资源
void cleanup_gui(void) {
    // 销毁渲染器
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    
    // 销毁窗口
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    
    // 退出SDL子系统
    SDL_Quit();
}

// 处理GUI事件，将事件放入队列
void handle_gui_events(EventQueue* event_queue) {
    if (!event_queue) return;
    
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        switch (e.type) {
            case SDL_QUIT:
                should_quit = 1;
                // 创建退出事件并放入队列
                Event* event = create_event(EVENT_QUIT);
                enqueue_event(event_queue, event);
                free_event(event);
                break;
                
            case SDL_KEYDOWN:
                // 创建按键按下事件并放入队列
                {
                    Event* event = create_event(EVENT_KEY_PRESS);
                    switch (e.key.keysym.sym) {
                        case SDLK_UP:
                            event->data.key.key = KEY_UP;
                            break;
                        case SDLK_DOWN:
                            event->data.key.key = KEY_DOWN;
                            break;
                        case SDLK_LEFT:
                            event->data.key.key = KEY_LEFT;
                            break;
                        case SDLK_RIGHT:
                            event->data.key.key = KEY_RIGHT;
                            break;
                        case SDLK_SPACE:
                            event->data.key.key = KEY_SPACE;
                            break;
                        case SDLK_ESCAPE:
                            event->data.key.key = KEY_ESC;
                            break;
                        default:
                            free_event(event);
                            event = NULL;
                            break;
                    }
                    
                    if (event) {
                        enqueue_event(event_queue, event);
                        free_event(event);
                    }
                }
                break;
                
            case SDL_KEYUP:
                // 创建按键释放事件并放入队列（这里可以忽略，因为我们只处理按下事件）
                break;
        }
    }
}

// 渲染坦克
static void render_tank(const Tank* tank, int is_player) {
    // 设置坦克颜色（玩家坦克为蓝色，AI坦克为红色）
    SDL_Color color = is_player ? COLOR_BLUE : COLOR_RED;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    // 绘制坦克主体（矩形）
    SDL_Rect tank_rect = {
        (int)tank->position.x - 20,
        (int)tank->position.y - 20,
        40, 40
    };
    SDL_RenderFillRect(renderer, &tank_rect);
    
    // 绘制坦克炮塔（根据角度）
    float angle_rad = tank->angle * M_PI / 180.0f;
    int turret_length = 30;
    int turret_x = (int)(tank->position.x + cosf(angle_rad) * turret_length);
    int turret_y = (int)(tank->position.y + sinf(angle_rad) * turret_length);
    
    SDL_SetRenderDrawColor(renderer, COLOR_YELLOW.r, COLOR_YELLOW.g, COLOR_YELLOW.b, COLOR_YELLOW.a);
    SDL_RenderDrawLine(renderer, (int)tank->position.x, (int)tank->position.y, turret_x, turret_y);
    
    // 绘制坦克生命值
    SDL_SetRenderDrawColor(renderer, COLOR_GREEN.r, COLOR_GREEN.g, COLOR_GREEN.b, COLOR_GREEN.a);
    SDL_Rect health_bar = {
        (int)tank->position.x - 20,
        (int)tank->position.y - 30,
        tank->health * 40 / 100, 5
    };
    SDL_RenderFillRect(renderer, &health_bar);
}

// 渲染墙体
static void render_block(const Block* block) {
    // 设置墙体颜色
    SDL_Color color = COLOR_BLACK;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    // 绘制墙体（矩形）
    SDL_Rect block_rect = {
        (int)block->position.x,
        (int)block->position.y,
        (int)block->width,
        (int)block->height
    };
    SDL_RenderFillRect(renderer, &block_rect);
}

// 渲染炮弹
static void render_shell(const Shell* shell) {
    // 设置炮弹颜色
    SDL_Color color = COLOR_YELLOW;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    // 绘制炮弹（圆形）
    int radius = 5;
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx * dx + dy * dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer, (int)shell->position.x + dx, (int)shell->position.y + dy);
            }
        }
    }
}

// 渲染游戏状态
void render_game_state(const GameState* state) {
    if (!state || !renderer) return;
    
    // 清空渲染器
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    
    // 渲染墙体
    for (int i = 0; i < state->block_count; i++) {
        render_block(&state->blocks[i]);
    }
    
    // 渲染坦克
    for (int i = 0; i < state->tank_count; i++) {
        render_tank(&state->tanks[i], i == 0);
    }
    
    // 渲染炮弹
    for (int i = 0; i < state->shell_count; i++) {
        render_shell(&state->shells[i]);
    }
    
    // 渲染游戏信息
    char info_text[100];
    sprintf(info_text, "分数: %d  时间: %d  生命值: %d", 
           state->tanks[0].score, state->game_time / 60, state->tanks[0].health);
    
    // 在实际应用中，这里应该使用SDL_ttf渲染文字
    // 为简化示例，这里仅打印在控制台
    printf("\r%s", info_text);
    
    // 如果游戏结束，显示游戏结束信息
    if (state->game_over) {
        printf("\n游戏结束！按ESC退出\n");
    }
    
    // 更新屏幕
    SDL_RenderPresent(renderer);
}

// 检查GUI是否请求退出
int gui_should_quit(void) {
    return should_quit;
}