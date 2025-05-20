#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdl_text.h"

// 屏幕尺寸
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

// 文本缓存数组
#define MAX_CACHE_ITEMS 100
TextCacheItem text_cache[MAX_CACHE_ITEMS];
int cache_count = 0;

// 初始化SDL和TTF
int init(SDL_Window** window, SDL_Renderer** renderer) {
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL初始化失败: %s\n", SDL_GetError());
        return 0;
    }

    // 初始化TTF
    if (TTF_Init() == -1) {
        printf("SDL_ttf初始化失败: %s\n", TTF_GetError());
        return 0;
    }

    // 创建窗口
    *window = SDL_CreateWindow("SDL2 文本渲染", 
                              SDL_WINDOWPOS_UNDEFINED, 
                              SDL_WINDOWPOS_UNDEFINED, 
                              SCREEN_WIDTH, SCREEN_HEIGHT, 
                              SDL_WINDOW_SHOWN);
    if (*window == NULL) {
        printf("窗口创建失败: %s\n", SDL_GetError());
        return 0;
    }

    // 创建硬件加速渲染器
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (*renderer == NULL) {
        printf("渲染器创建失败: %s\n", SDL_GetError());
        return 0;
    }

    return 1;
}

// 加载字体
TTF_Font* load_font(const char* font_path, int size) {
    TTF_Font* font = TTF_OpenFont(font_path, size);
    if (font == NULL) {
        printf("字体加载失败: %s\n", TTF_GetError());
    }
    return font;
}

// 查找缓存的文本纹理
SDL_Texture* find_cached_texture(const char* text, SDL_Color color, int font_size) {
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(text_cache[i].text, text) == 0 &&
            text_cache[i].color.r == color.r &&
            text_cache[i].color.g == color.g &&
            text_cache[i].color.b == color.b &&
            text_cache[i].color.a == color.a &&
            text_cache[i].font_size == font_size) {
            text_cache[i].hits++;
            return text_cache[i].texture;
        }
    }
    return NULL;
}

// 添加文本到缓存
void add_text_to_cache(const char* text, SDL_Color color, int font_size, SDL_Texture* texture) {
    int i = 0;
    unsigned int min_hits = 0;
    int min_hits_index = 0;

    if (cache_count < MAX_CACHE_ITEMS) {
        i = cache_count++;
set_cache:
        text_cache[i].text = strdup(text);
        text_cache[i].color = color;
        text_cache[i].font_size = font_size;
        text_cache[i].texture = texture;
        text_cache[i].hits = 0;
    } else { // 将缓存命中次数最少的那个移除出去
        for (i = 0; i < cache_count; i++) {
            if (0 == i) {
                min_hits = text_cache[i].hits;
                min_hits_index = 0;
                continue;
            }
            if (text_cache[i].hits < min_hits) {
                min_hits = text_cache[i].hits;
                min_hits_index = i;
            }
        }
        i = min_hits_index;
        free(text_cache[i].text);
        SDL_DestroyTexture(text_cache[i].texture);
        goto set_cache;
    }
}

void print_text_cache() {
    int i = 0;
    printf("Cached Text Table(count %d):\n", cache_count);
    for (i = 0; i < cache_count; i++) {
        printf("[%d] %s. hits:%u\n", i, text_cache[i].text, text_cache[i].hits);
    }
}

void clear_text_cache() {
    for (int i = 0; i < cache_count; i++) {
        free(text_cache[i].text);
        SDL_DestroyTexture(text_cache[i].texture);
    }
}

// 渲染文本（带缓存加速）
SDL_Texture* _render_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, SDL_Color color, unsigned char need_cache) {
    // 检查缓存
    SDL_Texture* cached_texture = NULL;
    
    if (need_cache) {
        cached_texture = find_cached_texture(text, color, TTF_FontHeight(font));
        if (cached_texture != NULL) {
            return cached_texture;
        }
    }

    // 使用TTF_RenderUTF8_Blended确保UTF-8支持
    SDL_Surface* text_surface = TTF_RenderUTF8_Blended(font, text, color);
    if (text_surface == NULL) {
        printf("文本渲染失败: %s\n", TTF_GetError());
        return NULL;
    }

    // 创建纹理
    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
    if (text_texture == NULL) {
        printf("纹理创建失败: %s\n", SDL_GetError());
    }

    // 释放表面并添加到缓存
    SDL_FreeSurface(text_surface);
    if (need_cache) {
        add_text_to_cache(text, color, TTF_FontHeight(font), text_texture);
    }
    
    return text_texture; //不做缓存返回的纹理使用完需要用户手动SDL_DestroyTexture()（或者调用clear_text()），否则会造成内存泄露！
}

// 获取文本尺寸
/*使用案例：
// 获取文本尺寸
int text_width, text_height;
get_text_size(font, "居中显示的文本", &text_width, &text_height);

// 计算居中位置
int center_x = (SCREEN_WIDTH - text_width) / 2;
int center_y = (SCREEN_HEIGHT - text_height) / 2;

// 渲染文本
SDL_Texture* centered_text = render_text_with_cache(renderer, font, "居中显示的文本", text_color);
draw_text(renderer, centered_text, center_x, center_y);*/
void get_text_size(TTF_Font* font, const char* text, int* width, int* height) {
    TTF_SizeText(font, text, width, height);
}

// 渲染文本到屏幕
void draw_text(SDL_Renderer* renderer, SDL_Texture* texture, int x, int y) {
    SDL_Rect dest_rect;
    SDL_QueryTexture(texture, NULL, NULL, &dest_rect.w, &dest_rect.h);
    dest_rect.x = x;
    dest_rect.y = y;
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
}

// 清理资源
void cleanup(SDL_Window* window, SDL_Renderer* renderer) {
    // 释放缓存的纹理
    clear_text_cache();
    
    // 销毁渲染器和窗口
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    
    // 退出TTF和SDL
    TTF_Quit();
    SDL_Quit();
}

#define MAX_FONTS 5
FontCacheItem font_cache[MAX_FONTS] = {0};

// 获取缓存的字体（或加载新字体），注意字体路径不能发生变化
TTF_Font* get_cached_font(const char* font_path, int size) {
    // 检查缓存
    for (int i = 0; i < MAX_FONTS; i++) {
        if (font_cache[i].size == size && font_cache[i].font != NULL) {
            return font_cache[i].font;
        }
    }

    // 缓存中没有，加载新字体
    TTF_Font* new_font = TTF_OpenFont(font_path, size);
    if (new_font == NULL) {
        return NULL;
    }

    // 找到一个空槽或替换最旧的字体
    for (int i = 0; i < MAX_FONTS; i++) {
        if (font_cache[i].font == NULL) {
            font_cache[i].size = size;
            font_cache[i].font = new_font;
            return new_font;
        }
    }

    // 如果缓存已满，替换第一个字体
    TTF_CloseFont(font_cache[0].font);
    font_cache[0].size = size;
    font_cache[0].font = new_font;
    return new_font;
}

// 清理资源时关闭所有缓存的字体
void clear_cached_font() {
    for (int i = 0; i < MAX_FONTS; i++) {
        if (font_cache[i].font != NULL) {
            TTF_CloseFont(font_cache[i].font);
        }
    }
}

/*使用缓存的字体案例（频繁加载字体会影响性能，因此我们创建了一个字体缓存系统）
    TTF_Font* font_16 = get_cached_font("path/to/font.ttf", 16);
    TTF_Font* font_24 = get_cached_font("path/to/font.ttf", 24);
    TTF_Font* font_36 = get_cached_font("path/to/font.ttf", 36);
    TTF_Font* font_48 = get_cached_font("path/to/font.ttf", 48);

    SDL_Texture* text1 = render_text_with_cache(renderer, font_16, "16px 文字", text_color);
    SDL_Texture* text2 = render_text_with_cache(renderer, font_24, "24px 文字", text_color);
    SDL_Texture* text3 = render_text_with_cache(renderer, font_36, "36px 文字", text_color);
    SDL_Texture* text4 = render_text_with_cache(renderer, font_48, "48px 文字", text_color);

    ...
    clear_text_cache();
    clear_cached_font();
*/

#if 0
int main(int argc, char* argv[]) { //gcc sdl_text.c -o font.exe `sdl2-config --cflags --libs` -lSDL2_ttf -lm -g -lbsd
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    
    // 初始化
    if (!init(&window, &renderer)) {
        return 1;
    }
    
    // 加载字体（需要替换为实际字体路径）
    // TTF_Font* font = load_font("../../assets/Microsoft_JhengHei.ttf", 24);
    TTF_Font* font = get_cached_font("../../assets/Microsoft_JhengHei.ttf", 24);
    if (font == NULL) {
        cleanup(window, renderer);
        return 1;
    }
    
    // 设置文本颜色
    SDL_Color text_color = {255, 255, 255, 255}; // 白色
    
    // 主循环标志
    int quit = 0;
    SDL_Event e;
    
    // 渲染文本
    render_text_with_cache(renderer, font, "高效文本渲染示例", text_color);
    SDL_Texture* title_texture = render_text_with_cache(renderer, font, "高效文本渲染示例", text_color);
    font = get_cached_font("../../assets/Microsoft_JhengHei.ttf", 12);
    SDL_Texture* body_texture = render_text_with_cache(renderer, font, "这是一个使用SDL2和SDL_ttf的C语言文本渲染示例", text_color);
    
    // 主循环
    while (!quit) {
        // 处理事件
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
        }
        
        // 清屏
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // 黑色背景
        SDL_RenderClear(renderer);
        
        // 绘制文本
        draw_text(renderer, title_texture, (SCREEN_WIDTH - 300) / 2, 50);
        draw_text(renderer, body_texture, (SCREEN_WIDTH - 600) / 2, 120);
        
        // 更新屏幕
        SDL_RenderPresent(renderer);
    }
    print_text_cache();

    // 释放字体
    // TTF_CloseFont(font);
    clear_cached_font();
    
    // 清理资源
    cleanup(window, renderer);
    
    return 0;
}
#endif