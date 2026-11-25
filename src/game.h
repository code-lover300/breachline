/* BREACHLINE - a side-view breach-and-clear game for PSP homebrew and PC.
 * The game draws into its own 480x272 pixel buffer (0xAARRGGBB) and mixes its
 * own 44.1 kHz stereo audio, so platform layers only read buttons, show the
 * buffer and feed the sound card. */
#ifndef BREACHLINE_GAME_H
#define BREACHLINE_GAME_H

#include <stdint.h>

#define SCREEN_W 480
#define SCREEN_H 272
#define AUDIO_RATE 44100

enum {
    BTN_LEFT   = 1 << 0,
    BTN_RIGHT  = 1 << 1,
    BTN_UP     = 1 << 2,
    BTN_DOWN   = 1 << 3,
    BTN_SHOOT  = 1 << 4,  /* PSP: Cross    */
    BTN_KICK   = 1 << 5,  /* PSP: Square   */
    BTN_ROLL   = 1 << 6,  /* PSP: Circle   */
    BTN_RELOAD = 1 << 7,  /* PSP: Triangle */
    BTN_START  = 1 << 8,
    BTN_SELECT = 1 << 9
};

void game_render_loading(uint32_t *fb);       /* simple screen shown before game_init */
void game_init(void);
void game_update(unsigned buttons);              /* once per frame at 60 Hz */
void game_render(uint32_t *fb);                  /* SCREEN_W * SCREEN_H pixels */
void game_audio_mix(int16_t *stereo, int frames);/* call from the audio thread */
int  game_wants_quit(void);

/* debugging / tools */
int  game_level_count(void);
void game_debug_start_level(int n);
int  game_debug_validate(void);   /* returns number of level problems */
int  game_debug_state(void);      /* 0 title .. see game.c */

#endif
