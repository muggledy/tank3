#include <stdlib.h>
#include <stdio.h>
#include "game_state.h"
#include <bsd/string.h>

IDPool* tk_idpool = NULL;
GameState tk_shared_game_state;

int init_idpool() {
    tk_idpool = id_pool_create(ID_POOL_SIZE);
    if (!tk_idpool) {
        return -1;
    }
    return 0;
}

void cleanup_idpool() {
     if (tk_idpool) {
        id_pool_destroy(tk_idpool);
        tk_idpool = NULL;
    }
}

Tank* create_tank(tk_uint8_t *name, Point pos, tk_float32_t angle_deg, tk_uint8_t role) {
    Tank *tank = NULL;
    Tank *t = NULL;
    tk_uint8_t tank_num = 0;

    TAILQ_FOREACH(t, &tk_shared_game_state.tank_list, chain) {
        tank_num++;
    }
    if (tank_num >= DEFAULT_TANK_MAX_NUM) {
        printf("Error: create tank(%s) failed for current tank num %u already >= DEFAULT_TANK_MAX_NUM(%u)\n", 
            name, tank_num, DEFAULT_TANK_MAX_NUM);
        return NULL;
    }
    if (tk_shared_game_state.my_tank && (TANK_ROLE_SELF == role)) {
        printf("Error: create my tank(%s) failed for it(%s) already exists\n", name, tk_shared_game_state.my_tank->name);
        return NULL;
    }

    tank = malloc(sizeof(Tank));
    if (!tank) {
        goto error;
    }
    memset(tank, 0, sizeof(Tank));

    strlcpy(tank->name, name, sizeof(tank->name));
    tank->id = id_pool_allocate(tk_idpool);
    if (!tank->id) {
        printf("Error: id_pool_allocate failed\n");
        goto error;
    }
    tank->position = pos;
    tank->angle_deg = angle_deg;
    tank->role = role;
    SET_FLAG(tank, flags, TANK_ALIVE);
    // tank->basic_color = (void *)((TANK_ROLE_SELF == tank->role) ? ID2COLORPTR(TK_BLUE) : ID2COLORPTR(TK_RED));
    tank->health = tank->max_health = (TANK_ROLE_SELF == tank->role) ? 250 : 250;
    tank->speed = TANK_INIT_SPEED;
    tank->max_shell_num = DEFAULT_TANK_SHELLS_MAX_NUM;
    TAILQ_INIT(&tank->shell_list);

    TAILQ_INSERT_HEAD(&tk_shared_game_state.tank_list, tank, chain);
    if (TANK_ROLE_SELF == tank->role) {
        tk_shared_game_state.my_tank = tank;
    }

    printf("create a tank(name:%s, id:%lu, total size:%luB, ExplodeEffect's size: %luB) success, total tank num %u\n", 
        tank->name, tank->id, sizeof(Tank), sizeof(tank->explode_effect), tank_num+1);
    return tank;
error:
    printf("Error: create tank %s failed\n", name);
    if (tank) {
        free(tank);
    }
    return NULL;
}

void delete_tank(Tank *tank) {
    Shell *shell = NULL;
    Shell *tmp = NULL;
    tk_uint8_t shell_num = 0;

    if (!tank) {
        return;
    }
    if (TANK_ROLE_SELF == tank->role) {
        tk_shared_game_state.my_tank = NULL;
    }
    TAILQ_FOREACH_SAFE(shell, &tank->shell_list, chain, tmp) {
        TAILQ_REMOVE(&tank->shell_list, shell, chain);
        free(shell);
        shell_num++;
    }
    printf("tank(id:%lu) %s(flags:%lu, score:%u, health:%u) is deleted, and free %u shells\n", 
        (tank)->id, (tank)->name, (tank)->flags, (tank)->score, (tank)->health, shell_num);
    free(tank);
}

void init_game_state() {
    TAILQ_INIT(&tk_shared_game_state.tank_list);
    tk_shared_game_state.game_time = 0;
    tk_shared_game_state.game_over = 0;
}

void cleanup_game_state() {
    Tank *tank = NULL;
    Tank *tmp = NULL;
    tk_uint8_t tank_num = 0;

    TAILQ_FOREACH_SAFE(tank, &tk_shared_game_state.tank_list, chain, tmp) {
        TAILQ_REMOVE(&tk_shared_game_state.tank_list, tank, chain);
        delete_tank(tank);
        tank_num++;
    }
    printf("total %u tanks are all freed\n", tank_num);
}