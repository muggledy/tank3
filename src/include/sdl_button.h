#ifndef __SDL_BUTTON_H__
    #define __SDL_BUTTON_H__

#include <SDL2/SDL.h>
#include "global.h"
#include "queue.h"
#include "tools.h"

typedef enum {
    BUTTON_NORMAL,    // 正常状态
    BUTTON_HOVER,     // 鼠标悬停
    BUTTON_PRESSED,   // 鼠标按下
    BUTTON_DISABLED   // 禁用状态(当前暂未使用)
} ButtonState;

typedef struct _Button {
    SDL_Rect rect;          // 按钮位置和大小
    int text_offset_x;
    int text_offset_y;
    ButtonState state;      // 当前状态
    char text[128];         // 按钮文本
    int textSize;           // 文本大小
    SDL_Color colors[4];    // 四种状态的颜色 {正常, 悬停, 按下, 禁用}
    SDL_Color textColor;    // 文本颜色
    void (*onClick)(void*, void*); // 点击回调函数
    void* callbackData;     // 回调函数参数
    TAILQ_ENTRY(_Button) chain;
    /*below is for user data*/
    unsigned int user_flag; // 用户自定义flag状态标记数据
} Button;

extern Button* create_button(int x, int y, int w, int h, int text_offset_x, int text_offset_y, 
                const char* text, void (*onClick)(void*, void*), void* callbackData);
extern void render_all_buttons(SDL_Renderer* renderer) ;
extern void handle_click_event_for_all_buttons(SDL_Event* event);
extern void cleanup_all_buttons();

#endif