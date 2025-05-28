#include "global.h"
#include "game_state.h"
#include "gui_tank.h"

int main() {
    int quit = 0;
    SDL_Event e;
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
    init_game_state();
    create_tank("muggledy", (Point){400,300}, 300, TANK_ROLE_SELF);
    if (!mytankptr) {
        goto out;
    }
    gui_init_tank(mytankptr);
    // 主循环
    while (!quit) {
        // 处理事件
        while (SDL_PollEvent(&e) != 0) {
            // 用户请求退出
            if (e.type == SDL_QUIT) {
                quit = 1;
            } else if (e.type == SDL_KEYDOWN) { // 处理键盘事件
                if (mytankptr->health > 0) {
                    mytankptr->health--;
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
                handle_key(mytankptr);
            }
        }

        // 渲染场景
        render_gui_scene();
        // 控制帧率
        SDL_Delay(30); // 约60FPS
    }
    ret = 0;
    print_text_cache();

out:
    cleanup_game_state();
    cleanup_idpool();
    cleanup_ttf();
    cleanup_music();
    cleanup_gui();
    return ret;
}