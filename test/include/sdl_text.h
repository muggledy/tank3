#ifndef __SDL_FONT_H__
    #define __SDL_FONT_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// 文本缓存项结构
typedef struct {
    char* text;
    SDL_Color color;
    int font_size;
    SDL_Texture* texture;
    unsigned int hits; // 缓存命中次数
} TextCacheItem;

// 字体缓存结构
typedef struct {
    int size;
    TTF_Font* font;
} FontCacheItem;

#define render_text_with_cache(renderer, font, text, color) _render_text(renderer, font, text, color, 1)
#define render_cached_text render_text_with_cache
extern void clear_text_cache();
#define clear_cached_text clear_text_cache

#define render_text(renderer, font, text, color) _render_text(renderer, font, text, color, 0)
#define clear_text(texture) SDL_DestroyTexture(texture)

extern void draw_text(SDL_Renderer* renderer, SDL_Texture* texture, int x, int y);

extern void print_text_cache();
extern void get_text_size(TTF_Font* font, const char* text, int* width, int* height);

extern TTF_Font* load_font(const char* font_path, int size);
#define clear_font(font) TTF_CloseFont(font)

extern TTF_Font* get_cached_font(const char* font_path, int size);
#define load_cached_font(font_path, size) get_cached_font(font_path, size)
extern void clear_cached_font();

#endif