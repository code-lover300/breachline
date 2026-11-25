/* BREACHLINE - shared internals between game, art, gfx and audio modules */
#ifndef BL_INTERNAL_H
#define BL_INTERNAL_H

#include "game.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif

/* ---- world layout ---- */
#define TILE     16
#define HUD_H    22
#define VIEW_H   (SCREEN_H - HUD_H)
#define NFLOORS  3
#define FLOOR_H  112
#define SLAB     10
#define ROOF     48
#define STREET   64
#define MARGIN   160
#define MAXW     64
#define CACHE_H  (ROOF + NFLOORS * FLOOR_H + STREET)
#define CACHE_MAXW (MAXW * TILE + 2 * MARGIN)
#define DOOR_H   60

static inline int floor_top(int f)    { return ROOF + f * FLOOR_H; }
static inline int floor_ground(int f) { return ROOF + (f + 1) * FLOOR_H - SLAB; }
static inline int tile_of(float x)    { return (int)floorf(x / TILE); }

/* ---- levels ---- */
enum { TH_HOUSE, TH_STORE, TH_OFFICE, TH_CLUB, TH_TOWER, TH_COUNT };
typedef struct {
    const char *name, *brief1, *brief2;
    int theme;
    const char *rows[NFLOORS];
} LevelDef;
extern const LevelDef LEVELS[];
extern const int NLEVELS;

/* live level state (owned by game.c) */
extern char    map[NFLOORS][MAXW];
extern uint8_t door_open[NFLOORS][MAXW];
extern int     W, cur_level, floor_active[NFLOORS];

/* ---- gfx: drawing into a target buffer ---- */
typedef struct { uint32_t *px; int w, h; } Target;
extern Target *TG;
extern int TOX, TOY;
void tg_set(Target *t, int ox, int oy);
void tg_clip(int x0, int y0, int x1, int y1);
int  tg_clip_y1(void);

uint32_t cmul(uint32_t c, int k);            /* k: 256 = 1.0 */
uint32_t cmix(uint32_t a, uint32_t b, int t);/* t: 0..256 toward b */
uint32_t cadd(uint32_t d, uint32_t s, int a);
uint32_t hash32(uint32_t x);

void px_set(int x, int y, uint32_t c);
void fill(int x, int y, int w, int h, uint32_t c);
void fill_a(int x, int y, int w, int h, uint32_t c, int a);
void fill_add(int x, int y, int w, int h, uint32_t c, int a);
void vgrad(int x, int y, int w, int h, uint32_t top, uint32_t bot);
void disc(float cx, float cy, float r, uint32_t c);
void disc_a(float cx, float cy, float r, uint32_t c, int a);
void disc_cap(float cx, float cy, float r, uint32_t c, float ymax);
void capsule(float x0, float y0, float x1, float y1, float r, uint32_t c);
void line_a(float x0, float y0, float x1, float y1, uint32_t c, int a);
void line_add(float x0, float y0, float x1, float y1, uint32_t c, int a);
void glow(float cx, float cy, float r, uint32_t c, int a);
void ring(float cx, float cy, float r, uint32_t c);

int  text_w(const char *s, int sc);
void text_raw(int x, int y, const char *s, uint32_t c, int sc);
void text(int x, int y, const char *s, uint32_t c, int sc);
void text_c(int cx, int y, const char *s, uint32_t c, int sc);
void text_grad(int x, int y, const char *s, uint32_t top, uint32_t bot, int sc);

/* ---- art ---- */
extern Target CACHE_LIT, CACHE_DARK;
extern uint8_t cache_row_alpha[CACHE_H];
extern int light_col[NFLOORS][MAXW * TILE];
void art_bake(void);
void art_sky(Target *t, float camx, float camy, float time);
void art_add_bullet_hole(int f, float wx, int wy);

typedef struct {
    uint32_t skin, hair, helm, visor, top, top2, accent, pants, boots, gun;
    int kind; /* 0 operator, 1 guard, 2 heavy, 3 hostage */
} Look;
typedef struct {
    float walk;     /* phase, radians */
    float move;     /* 0 idle .. 1 walking */
    float kick;     /* 0..1 */
    float recoil;   /* 0..1 */
    float roll;     /* -1 off, else 0..1 */
    float fall;     /* 0 standing .. 1 lying */
    float aim;      /* 0 lowered .. 1 raised */
    float t;        /* time, for idle sway */
    int   stunned;
    int   secured;  /* hostages */
} Pose;
extern const Look LOOK_OPERATOR, LOOK_GUARD, LOOK_HEAVY, LOOK_HOSTAGE;
/* returns muzzle position (screen space) through mx,my if non-null */
void draw_rig(float cx, float gy, int dir, const Look *lk, const Pose *ps, int light,
              uint32_t flash, float scale, float *mx, float *my);
void draw_sprite(int kind, int frame, float cx, float gy, int dir, int light, uint32_t flash, int sc,
                 float *mx, float *my);
void draw_door(int sx, int g, float open, int dir, int light, int broken);
void draw_minimap(int lvl, int x, int y, int cell);

/* ---- audio ---- */
enum {
    SFX_SHOT, SFX_ESHOT, SFX_HEAVYSHOT, SFX_KICK, SFX_DOOR, SFX_HIT, SFX_HURT, SFX_MAGOUT,
    SFX_MAGIN, SFX_DRY, SFX_STEP, SFX_ROLL, SFX_SECURE, SFX_UI, SFX_UI_OK, SFX_CLEAR,
    SFX_FAIL, SFX_CASING, SFX_STAR, SFX_ALERT, SFX_STAIRS, SFX_BODY, SFX_COUNT
};
void sfx(int id, float pan, float vol);
void music_set(int track); /* 0 off, 1 title, 2 mission */

#endif
