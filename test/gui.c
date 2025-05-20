
#if 0
int main() { //gcc tank.c -o tank `sdl2-config --cflags --libs` -lm -g -lbsd
    int quit = 0;
    SDL_Event e;
    tk_float32_t new_dir = 0;
    Tank *tank = NULL;
    int ret = -1;

    // 结合时间和进程ID作为随机种子（用于爆炸粒子）
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    if (init_gui() != 0){
        goto out;
    }
    tk_idpool = id_pool_create(ID_POOL_SIZE);
    if (!tk_idpool) {
        goto out;
    }
    tank = create_tank("muggledy", (Point){400,300}, 300, TANK_ROLE_SELF);
    if (!tank) {
        goto out;
    }
    // 主循环
    while (!quit) {
        // 处理事件
        while (SDL_PollEvent(&e) != 0) {
            // 用户请求退出
            if (e.type == SDL_QUIT) {
                quit = 1;
            } else if (e.type == SDL_KEYDOWN) { // 处理键盘事件
                if (tank->health > 0) {
                    tank->health--;
                } else {
                    break;
                }
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = 1;
                        break;
                    case SDLK_w:
                        tank->position = move_point(tank->position, tank->angle_deg, tank->speed);
                        break;
                    case SDLK_s:
                        new_dir = tank->angle_deg + 180;
                        if (new_dir > 360) {
                            new_dir -= 360;
                        }
                        tank->position = move_point(tank->position, new_dir, tank->speed);
                        break;
                    case SDLK_a:
                        tank->angle_deg += 360;
                        tank->angle_deg -= 10;
                        if (tank->angle_deg >= 360) {
                            tank->angle_deg -= 360;
                        }
                        break;
                    case SDLK_d:
                        tank->angle_deg += 10;
			            if (tank->angle_deg >= 360) {
                            tank->angle_deg -= 360;
                        }
                        break;
                }
            }
        }

        // 渲染场景
        render_gui_scene(tank);
        // 控制帧率
        SDL_Delay(30); // 约60FPS
    }
    ret = 0;

out:
    cleanup_gui();
    delete_tank(&tank);
    if (tk_idpool) {
        id_pool_destroy(tk_idpool);
        tk_idpool = NULL;
    }
    return ret;
}
#endif