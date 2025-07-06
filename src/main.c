#define _GNU_SOURCE
#include "global.h"
#include "game_state.h"
#include "gui_tank.h"
#include "event_loop.h"
#include "debug.h"
#include <sched.h>
#include "tools.h"

// #define RUN_ON_MULTI_CORE // 设置了反而效果不好，因为明面上我只有三个线程（含主线程），但实际
// 一些三方库隐含创建了多线程，因此本游戏实际涉及>3个线程，设置RUN_ON_MULTI_CORE会使得线程集中于两个核心上，
// 不设置，Linux会动态平衡线程在不同核心上的负载，更均匀分布线程到>3个core上，以最大化利用多核CPU资源，
// 避免某个核心过载

void pin_thread_to_cpu(pthread_t thread, int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);          // 清空 CPU 集合
    CPU_SET(cpu_id, &cpuset);   // 绑定到指定 CPU

    // 设置线程的 CPU 亲和性
    int ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (ret != 0) {
        perror("pthread_setaffinity_np failed");
    }
}

void* gui_thread(void* arg) {
    reset_debug_prefix("gui");
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
    create_tank("yangdai", get_random_grid_pos_for_tank(), 300, TANK_ROLE_SELF);
    create_tank("muggle-0", get_random_grid_pos_for_tank(), random_range(0, 360), TANK_ROLE_ENEMY_MUGGLE);
    if (!mytankptr) {
        goto out;
    }
    gui_init_all_tank();
    create_button(tk_maze_offset.x+GRID_SIZE*HORIZON_GRID_NUMBER+10, tk_maze_offset.y, 50, 30, 8, 2, 
        "暂停", stop_game_button_click_callback, NULL);
    // 主循环
    gui_main_loop();
    // ret = 0;
    print_text_cache();

out:
    cleanup_all_buttons();
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
#if defined(RUN_ON_MULTI_CORE)
    int cpu1 = 0;  // 绑定到 CPU 0
    int cpu2 = 1;  // 绑定到 CPU 1
    if (pthread_create(&control_tid, NULL, control_thread, &cpu1) != 0) {
#else
    if (pthread_create(&control_tid, NULL, control_thread, NULL) != 0) {
#endif
        tk_debug("Error: failed to create Control thread\n");
        return -1;
    }
#if defined(RUN_ON_MULTI_CORE)
    if (pthread_create(&gui_tid, NULL, gui_thread, &cpu2) != 0) {
#else
    if (pthread_create(&gui_tid, NULL, gui_thread, NULL) != 0) {
#endif
        tk_debug("Error: failed to create GUI thread\n");
        // stop_event_loop(); // Libevent的event_base不是线程安全的，直接跨线程调用event_base_loopbreak()会导致未定义行为，
        // 可能无法停止控制线程事件主循环，所以应当使用管道或其他线程间通信方式通知控制线程自己调用stop_event_loop()来结束事件循环
        notify_control_thread_exit();
        pthread_join(control_tid, NULL);
        return -1;
    }
    // 设置 CPU 亲和性（通过 ps -eLo pid,tid,psr,cmd | grep tank.exe 观察效果）
#if defined(RUN_ON_MULTI_CORE)
    pin_thread_to_cpu(control_tid, cpu1);
    pin_thread_to_cpu(gui_tid, cpu2);
#endif

    tk_debug("Note: you can close GUI window to end the game...\n"); // 叉掉或按Esc关闭GUI窗口，将结束控制线程的事件主循环，两个线程就都可以结束了
    // 等待线程结束
    pthread_join(control_tid, NULL);
    pthread_join(gui_tid, NULL);
    tk_debug("game over(%us)!\n", ((tk_shared_game_state.game_time * RENDER_FPS_MS) / 1000));
    return 0;
}