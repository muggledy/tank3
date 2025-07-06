#include "sdl_button.h"
#include <stdlib.h>
#include "sdl_text.h"
#ifdef BUTTON_LOCK
#include <pthread.h>
#endif

#ifdef BUTTON_LOCK
extern void lock(pthread_spinlock_t *spinlock);
extern void unlock(pthread_spinlock_t *spinlock);
#endif

TAILQ_HEAD(_tk_buttons_list, _Button) tk_button_list = TAILQ_HEAD_INITIALIZER(tk_button_list);
#ifdef BUTTON_LOCK
pthread_spinlock_t tk_button_list_op_spinlock; // 参考tank->spinlock
#endif

Button* create_button(int x, int y, int w, int h, int text_offset_x, int text_offset_y, const char* text, 
                     void (*onClick)(void*, void*), void* callbackData) { //x/y是按钮左上角，w/h是按钮宽度和高度，text_offset_x/y是文字左上角相较于按钮左上角的偏移量
    Button* button = (Button*)malloc(sizeof(Button));
    if (!button) return NULL;
    
    button->rect = (SDL_Rect){x, y, w, h};
    button->text_offset_x = text_offset_x;
    button->text_offset_y = text_offset_y;
    button->state = BUTTON_NORMAL;
    strlcpy(button->text, text, sizeof(button->text));
    button->textSize = 16;
    button->onClick = onClick;
    button->callbackData = callbackData;
    
    // 设置四种状态的颜色
    button->colors[BUTTON_NORMAL] = (SDL_Color){100, 100, 100, 255};  // 深灰色
    button->colors[BUTTON_HOVER] = (SDL_Color){150, 150, 150, 255};   // 中灰色
    button->colors[BUTTON_PRESSED] = (SDL_Color){200, 200, 200, 255}; // 浅灰色
    button->colors[BUTTON_DISABLED] = (SDL_Color){50, 50, 50, 128};   // 暗灰色（半透明）
    button->textColor = (SDL_Color){255, 255, 255, 255}; // 白色文本
#ifdef BUTTON_LOCK
    lock(&tk_button_list_op_spinlock);
#endif
    TAILQ_INSERT_HEAD(&tk_button_list, button, chain);
#ifdef BUTTON_LOCK
    unlock(&tk_button_list_op_spinlock);
#endif
    return button;
}

void destroy_button(Button* button) {
    if (!button) return;
#ifdef BUTTON_LOCK
    lock(&tk_button_list_op_spinlock);
#endif
    TAILQ_REMOVE(&tk_button_list, button, chain);
#ifdef BUTTON_LOCK
    unlock(&tk_button_list_op_spinlock);
#endif
    free(button);
}

// 渲染按钮
void render_button(SDL_Renderer* renderer, Button* button) {
    TTF_Font *button_font = NULL;
    SDL_Texture *button_text = NULL;
    if (!button || !renderer) return;

    if ((0 != button->rect.w) && (0 != button->rect.h)) {
        // 绘制按钮背景
        SDL_SetRenderDrawColor(renderer, 
                            button->colors[button->state].r,
                            button->colors[button->state].g,
                            button->colors[button->state].b,
                            button->colors[button->state].a);
        SDL_RenderFillRect(renderer, &button->rect);

        // 绘制按钮边框
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &button->rect);
    }

    // 如果有字体，绘制按钮文本
    if (button->text) {
        button_font = load_cached_font(DEFAULT_FONT_PATH, button->textSize);
        if ((0 == button->rect.w) || (0 == button->rect.h)) {
            get_text_size(button_font, button->text, &button->rect.w, &button->rect.h); // 宽度获取不准确
            printf("render button(%s) at pos(%d,%d) with size(%d,%d)\n", button->text, 
                button->rect.x, button->rect.y, button->rect.w, button->rect.h);
        }
        button_text = render_cached_text(renderer, button_font, button->text, button->textColor);
        draw_text(renderer, button_text, button->rect.x+button->text_offset_x, button->rect.y+button->text_offset_y);
    }
}

// 处理按钮事件
void handle_button_event(Button* button, SDL_Event* event) {
    if (!button || button->state == BUTTON_DISABLED) return;
    
    int mouseX, mouseY;
    Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
    
    if ((0 == button->rect.w) || (0 == button->rect.h)) {
        return;
    }
    // 检查鼠标是否在按钮区域内
    int isInside = ((mouseX >= button->rect.x) && (mouseX < button->rect.x + button->rect.w) &&
                    (mouseY >= button->rect.y) && (mouseY < button->rect.y + button->rect.h));
    
    switch (event->type) {
        case SDL_MOUSEMOTION:
            button->state = isInside ? BUTTON_HOVER : BUTTON_NORMAL;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (isInside && event->button.button == SDL_BUTTON_LEFT) {
                button->state = BUTTON_PRESSED;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (button->state == BUTTON_PRESSED) {
                if (isInside) {
                    // 触发点击回调
                    if (button->onClick) {
                        button->onClick((void *)button, button->callbackData);
                    }
                }
                button->state = isInside ? BUTTON_HOVER : BUTTON_NORMAL;
            }
            break;
    }
}

void render_all_buttons(SDL_Renderer* renderer) {
    Button *button = NULL;
#ifdef BUTTON_LOCK
    lock(&tk_button_list_op_spinlock);
#endif
    TAILQ_FOREACH(button, &tk_button_list, chain) {
        render_button(renderer, button);
    }
#ifdef BUTTON_LOCK
    unlock(&tk_button_list_op_spinlock);
#endif
}

void handle_event_for_all_buttons(SDL_Event* event) {
    Button *button = NULL;
#ifdef BUTTON_LOCK
    lock(&tk_button_list_op_spinlock);
#endif
    TAILQ_FOREACH(button, &tk_button_list, chain) {
        handle_button_event(button, event);
    }
#ifdef BUTTON_LOCK
    unlock(&tk_button_list_op_spinlock);
#endif
}

void cleanup_all_buttons() {
    Button *button = NULL;
    Button *tmp = NULL;
#ifdef BUTTON_LOCK
    lock(&tk_button_list_op_spinlock);
#endif
    TAILQ_FOREACH_SAFE(button, &tk_button_list, chain, tmp) {
        TAILQ_REMOVE(&tk_button_list, button, chain);
        free(button);
    }
#ifdef BUTTON_LOCK
    unlock(&tk_button_list_op_spinlock);
#endif
}