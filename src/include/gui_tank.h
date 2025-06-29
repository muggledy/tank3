#ifndef __GUI_TANK_H__
    #define __GUI_TANK_H__

#include <SDL2/SDL.h>
#include "global.h"
#include <SDL2/SDL_mixer.h>
#include "game_state.h"
#include <stdlib.h>  // 提供 srand() 和 rand() 等声明
#include <time.h>    // 提供 time() 等声明
#include <unistd.h>  // 提供 getpid() 等声明
#include "sdl_text.h"

typedef struct {
    Mix_Chunk* sound;
    int channel;
} MusicEntry;

typedef struct {
#define DEFAULT_TANK_MOVE_MUSIC_PATH "./assets/tank_move.wav"
    MusicEntry move;
#define DEFAULT_TANK_EXPLODE_MUSIC_PATH "./assets/tank_explode.wav"
    MusicEntry explode;
#define DEFAULT_TANK_SHOOT_MUSIC_PATH "./assets/tank_shoot.wav"
    MusicEntry shoot;
#define DEFAULT_TANK_HIT_MUSIC_PATH "./assets/tank_hit.wav" // download from https://www.tukuppt.com/yinxiaomuban/tanke.html
    MusicEntry hit;
} TankMusic;

extern KeyValue tk_key_value;
extern TankMusic tk_music;

#define DEFAULT_FONT_PATH "./assets/Microsoft_JhengHei.ttf"

// 颜色定义
// 定义颜色枚举
typedef enum {
    TK_WHITE,
    TK_BLACK,
    TK_RED,
    TK_GREEN,
    TK_BLUE,
    TK_YELLOW
} TKColorID;

extern SDL_Color tk_colors[];
#define ID2COLOR(colorid)    tk_colors[colorid]
#define ID2COLORPTR(colorid) &(tk_colors[colorid])

#define COLOR2PARAM(color) \
    (color).r,(color).g,(color).b,(color).a
#define COLORPTR2PARAM(colorptr) \
    (colorptr)->r,(colorptr)->g,(colorptr)->b,\
    (colorptr)->a
#define COLORPTR2PARAM2(colorptr,alpha) \
    (colorptr)->r,(colorptr)->g,(colorptr)->b,\
    (int)(((colorptr)->a)*(alpha))

#define SET_COLOR(color, r, g, b, a) do { \
    (color).r = (r); \
    (color).g = (g); \
    (color).b = (b); \
    (color).a = (a); \
} while(0)

extern int play_music(MusicEntry *music, int only_once);
extern void pause_music(MusicEntry *music);

#define TANKCOLORPTR(tankptr) ((SDL_Color *)((tankptr)->basic_color))

#define PLAY_MOVE_MUSIC() \
do { \
    if (!TST_FLAG(&tk_key_value, mask, TK_KEY_W_ACTIVE) && !TST_FLAG(&tk_key_value, mask, TK_KEY_S_ACTIVE) \
        && !TST_FLAG(&tk_key_value, mask, TK_KEY_A_ACTIVE) && !TST_FLAG(&tk_key_value, mask, TK_KEY_D_ACTIVE)) { \
        play_music(&(tk_music.move), 0); \
    } \
} while(0);

#define PAUSE_MOVE_MUSIC() \
do { \
    if (!TST_FLAG(&tk_key_value, mask, TK_KEY_W_ACTIVE) && !TST_FLAG(&tk_key_value, mask, TK_KEY_S_ACTIVE) \
        && !TST_FLAG(&tk_key_value, mask, TK_KEY_A_ACTIVE) && !TST_FLAG(&tk_key_value, mask, TK_KEY_D_ACTIVE)) { \
        pause_music(&(tk_music.move)); \
    } \
} while(0);

extern int init_gui(void);
extern void cleanup_gui(void);
extern int init_music();
extern void cleanup_music();
void render_gui_scene();
extern void render_tank(SDL_Renderer* renderer, Tank* tank);
#define draw_tank render_tank
extern int check_resource_file();
extern void gui_init_tank(Tank *tank);
extern void gui_init_all_tank();

extern void notify_control_thread_exit();
// extern void send_key_to_control_thread(int key_type, int key_value);
extern void gui_main_loop();

#endif