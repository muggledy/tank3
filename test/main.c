#include "global.h"
#include "game_state.h"
#include "gui_tank.h"
#include "event_loop.h"
#include "debug.h"

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

void* gui_thread(void* arg) {
    reset_debug_prefix("gui");

    int quit = 0;
    SDL_Event e;
    // int ret = -1;

    tk_debug("GUI thread started...\n");
    // 确认游戏资源是否存在
    if (check_resource_file() != 0) {
        return NULL; //ret;
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
                // stop_event_loop();
                notify_control_thread_exit();
            } else if (e.type == SDL_KEYDOWN) { // 处理键盘事件
                if (mytankptr->health > 0) {
                    mytankptr->health--;
                } else {
                    break;
                }
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = 1;
                        // stop_event_loop();
                        notify_control_thread_exit();
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
    // ret = 0;
    print_text_cache();

out:
    cleanup_game_state();
    cleanup_idpool();
    cleanup_ttf();
    cleanup_music();
    cleanup_gui();
    // return ret;
    tk_debug("GUI thread exit success!\n");
    return NULL;
}

// 控制线程函数
void* control_thread(void* arg) {
    reset_debug_prefix("control");

    // 初始化libevent
    if (init_event_loop() != 0) {
        return NULL;
    }

    tk_debug("Control thread main loop started...\n");
    // 事件主循环将持续运行，直至调用stop_event_loop()来退出循环
    run_event_loop();

    // 清理事件相关资源
    cleanup_event_loop();
    tk_debug("Control thread exit success!\n");
    return NULL;
}

int main() {
    reset_debug_prefix("main");

    // 创建线程
    pthread_t control_tid, gui_tid;
    if (pthread_create(&control_tid, NULL, control_thread, NULL) != 0) {
        tk_debug("Error: failed to create Control thread\n");
        return -1;
    }
    if (pthread_create(&gui_tid, NULL, gui_thread, NULL) != 0) {
        tk_debug("Error: failed to create GUI thread\n");
        // stop_event_loop(); // Libevent的event_base不是线程安全的，直接跨线程调用event_base_loopbreak()会导致未定义行为，
        // 可能无法停止控制线程事件主循环，所以应当使用管道或其他线程间通信方式通知控制线程自己调用stop_event_loop()来结束事件循环
        notify_control_thread_exit();
        pthread_join(control_tid, NULL);
        return -1;
    }

    tk_debug("Note: you can close GUI window to end the game...\n"); // 叉掉或按Esc关闭GUI窗口，将结束控制线程的事件主循环，两个线程就都可以结束了
    // 等待线程结束
    pthread_join(control_tid, NULL);
    pthread_join(gui_tid, NULL);
    tk_debug("game over!\n");
    return 0;
}