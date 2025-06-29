#ifndef __SDL_FONT_H__
    #define __SDL_FONT_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "hashtbl.h"

#define FONT_CACHE_BASED_ON_HASH // 若不定义该宏，则使用基于数组的缓存表，性能较差(体现倒也不明显)，建议使用哈希表实现缓存表

// 文本缓存项结构
typedef struct _TextCacheItem {
    char* text;
    SDL_Color color;
    int font_size;
    SDL_Texture* texture;
    unsigned int hits; // 缓存命中次数
#ifdef FONT_CACHE_BASED_ON_HASH
    tk_uint32_t hashkey;
    hashtbl_link_t hashlink;
#endif
} TextCacheItem;

// 字体缓存结构
typedef struct {
    int size;
    TTF_Font* font;
} FontCacheItem;

extern int init_ttf();
extern void cleanup_ttf();

//not recommended start
extern SDL_Texture* _render_text(SDL_Renderer* renderer, 
    TTF_Font* font, const char* text, SDL_Color color, unsigned char need_cache);
//end

#define render_text_with_cache(renderer, font, text, color) _render_text(renderer, font, text, color, 1)
#define render_cached_text render_text_with_cache
extern void clear_text_cache();
#define clear_cached_text clear_text_cache

//not recommended start
#define render_text(renderer, font, text, color) _render_text(renderer, font, text, color, 0)
#define clear_text(texture) SDL_DestroyTexture(texture)
//end

extern void draw_text(SDL_Renderer* renderer, SDL_Texture* texture, int x, int y);

extern void print_text_cache();
extern void get_text_size(TTF_Font* font, const char* text, int* width, int* height);

//not recommended start
extern TTF_Font* load_font(const char* font_path, int size);
#define clear_font(font) TTF_CloseFont(font)
//end

extern TTF_Font* get_cached_font(const char* font_path, int size);
#define load_cached_font get_cached_font
extern void clear_cached_font();

#endif