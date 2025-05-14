#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

#include "event_queue.h"
#include "game_state.h"
#include "controller.h"
#include "gui.h"
#include "network.h"

// 程序状态
static atomic_bool running = ATOMIC_VAR_INIT(true);

// 控制线程函数
void* control_thread(void* arg) {
    EventQueue* event_queue = (EventQueue*)arg;
    
    // 初始化libevent
    init_event_loop(event_queue);
    
    // 启动网络（如果是联网模式）
    start_network();
    
    // 事件循环将持续运行，直到调用stop_event_loop()
    run_event_loop();
    
    // 清理资源
    cleanup_network();
    cleanup_event_loop();
    
    return NULL;
}

// GUI线程函数
void* gui_thread(void* arg) {
    EventQueue* event_queue = (EventQueue*)arg;
    GameState game_state;
    
    // 初始化GUI
    init_gui();
    
    while(atomic_load(&running)) {
        // 处理GUI事件（如用户输入）
        handle_gui_events(event_queue);
        
        // 获取最新游戏状态
        get_game_state(&game_state);
        
        // 渲染游戏画面
        render_game_state(&game_state);
        
        // 检查游戏是否结束
        if (game_state.game_over) {
            atomic_store(&running, false);
            stop_event_loop(); // 停止事件循环
        }
        
        // 等待一小段时间
        usleep(16000); // 约60FPS
    }
    
    // 清理GUI资源
    cleanup_gui();
    
    return NULL;
}

int main() {
    // 创建事件队列
    EventQueue* event_queue = create_event_queue();
    if (!event_queue) {
        fprintf(stderr, "Failed to create event queue\n");
        return 1;
    }
    
    // 初始化控制器
    init_controller(event_queue);
    
    // 创建线程
    pthread_t control_tid, gui_tid;
    if (pthread_create(&control_tid, NULL, control_thread, event_queue) != 0) {
        fprintf(stderr, "Failed to create control thread\n");
        destroy_event_queue(event_queue);
        return 1;
    }
    
    if (pthread_create(&gui_tid, NULL, gui_thread, event_queue) != 0) {
        fprintf(stderr, "Failed to create GUI thread\n");
        atomic_store(&running, false);
        stop_event_loop();
        pthread_join(control_tid, NULL);
        destroy_event_queue(event_queue);
        return 1;
    }
    
    // 等待用户终止程序
    printf("Press Enter to exit...\n");
    getchar();
    
    // 停止程序
    atomic_store(&running, false);
    stop_event_loop();
    
    // 等待线程结束
    pthread_join(control_tid, NULL);
    pthread_join(gui_tid, NULL);
    
    // 清理资源
    cleanup_controller();
    destroy_event_queue(event_queue);
    
    return 0;
}