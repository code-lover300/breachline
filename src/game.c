/* BREACHLINE - gameplay, camera, effects, HUD, menus. Original game and art. */
#include "internal.h"
#include "sprites_gen.h"

/* ------------------------------------------------------------------ */
/* levels                                                              */
/* '#' wall  '.' room  'D' door  'S' stairs (line up between floors)   */
/* 'P' player  'E' guard  'W' patrolling guard  'B' heavy  'H' hostage */
/* A floor that is just "#" is not used and is drawn as outside wall.  */
/* ------------------------------------------------------------------ */
const LevelDef LEVELS[] = {
    { "TRAINING HOUSE", "KICK DOORS TO STUN WHOEVER IS BEHIND THEM.",
      "CLEAR EVERY ROOM. DO NOT HIT THE HOSTAGE.", TH_HOUSE,
      { "#",
        "#",
        "#P......D..........E......D.........E.....D......W.....D...H..#" } },
    { "CORNER STORE", "TWO FLOORS. UP AND DOWN ON THE STAIRS.",
      "THE HEAVY IN OLIVE TAKES THREE HITS.", TH_STORE,
      { "#",
        "#.....E.......D..........S......D.....W.......H....#",
        "#P........D...E..........S...........B........E....#" } },
    { "HARBOR OFFICES", "THREE FLOORS, TWO HOSTAGES.",
      "ROLL WITH CIRCLE TO SLIP PAST INCOMING FIRE.", TH_OFFICE,
      { "#...H....B......D..........S.........D.......E.....E...#",
        "#......E........D....W.....S....D........S.......E.....#",
        "#P..........D........E...........B.......S.......H.....#" } },
    { "NIGHT CLUB", "LOUD, CROWDED, AND A HOSTAGE RIGHT BEHIND",
      "A GUARD. ONE CLEAN SHOT. NOT TWO.", TH_CLUB,
      { "#",
        "#.......B....D...H.........S.......D.......E...W...E.......#",
        "#P.....D.....E....E......D.S..........W.........D....E..H..#" } },
    { "THE TOWER", "FINAL OPERATION. HEAVIES ON EVERY FLOOR.",
      "GET BOTH HOSTAGES OUT. GOOD LUCK.", TH_TOWER,
      { "#..H.....B.......D.......W.....S.......D.....B.....H...#",
        "#.....E......D.........E.......S...D.......S.....E.....#",
        "#P......D......W.......D....E..........B...S...........#" } },
};
const int NLEVELS = (int)(sizeof(LEVELS) / sizeof(LEVELS[0]));

/* ------------------------------------------------------------------ */
/* state                                                               */
/* ------------------------------------------------------------------ */
#define DT       (1.0f / 60.0f)
#define MAXE     32
#define MAXHOST  8
#define MAXB     64
#define MAXP     400
#define MAXCAS   96
#define MAXFT    12
#define MAG_SIZE 8

#define ET_GRUNT  0
#define ET_WALKER 1
#define ET_HEAVY  2

enum { ST_TITLE, ST_SELECT, ST_CONTROLS, ST_BRIEF, ST_PLAY, ST_PAUSE, ST_CLEAR, ST_FAIL, ST_WIN };
enum { PT_SPARK, PT_DEBRIS, PT_DUST, PT_SMOKE, PT_DROP, PT_FLASH };

typedef struct {
    float x; int f, dir, type, hp, alive, aware, alerted, breached;
    float react, fire, stun, flash, deadt, muzzle, walk, move, pmin, pmax, turn_t, recoil, bang;
} Enemy;
typedef struct { float x; int f, state; float t; } Hostage;
typedef struct { float x, y, vx; int f, on, mine; } Bullet;
typedef struct { float x, y, vx, vy, life, max, size; uint32_t c; int type, f; } Part;
typedef struct { float x, y, vx, vy; int f, on, rest; } Casing;
typedef struct { float x, y, t; char s[24]; uint32_t c; } FText;
typedef struct {
    float x; int f, dir, hp, ammo;
    float reload, roll, roll_cd, kick, inv, shoot_cd, muzzle, walk, move, hurt, recoil, stair_t, step_acc;
    int stair_to;
} Player;

char    map[NFLOORS][MAXW];
uint8_t door_open[NFLOORS][MAXW];
int     W, cur_level, floor_active[NFLOORS];
static float  door_anim[NFLOORS][MAXW];
static int8_t door_dir[NFLOORS][MAXW];

static int      state = ST_TITLE, menu_sel, select_sel, pause_sel, quit_req;
static unsigned btn, prev_btn;
static float    state_t, play_t, clear_delay, shake, gtime, flash_white;
static int      hitstop, stars_shown;
static char     fail_msg[48];
static int      shots, hits, damage_taken, kills, breach_kills, score, stars_now;

static Player   P;
static Enemy    E[MAXE];    static int nE;
static Hostage  H[MAXHOST]; static int nH;
static Bullet   B[MAXB];
static Part     PT[MAXP];
static Casing   CS[MAXCAS];
static FText    FT[MAXFT];
static float    camx, camy;
static int      camxi, camyi;

typedef struct { char magic[4]; uint8_t stars[8]; int32_t best[8]; uint8_t unlocked, pad[3]; } Save;
static Save save;

static Target SCR;
static uint32_t rng = 0x1234567u;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static float frand(float a, float b) { return a + (b - a) * (float)(rnd() & 0xFFFF) / 65535.0f; }

#define PRESSED(b) ((btn & (b)) && !(prev_btn & (b)))

/* ------------------------------------------------------------------ */
/* save data                                                           */
/* ------------------------------------------------------------------ */
static void save_load(void) {
    memset(&save, 0, sizeof save);
    FILE *f = fopen("breachline.sav", "rb");
    if (f) { if (fread(&save, sizeof save, 1, f) != 1) memset(&save, 0, sizeof save); fclose(f); }
    if (memcmp(save.magic, "BRL2", 4)) { memset(&save, 0, sizeof save); memcpy(save.magic, "BRL2", 4); }
    if (save.unlocked < 1) save.unlocked = 1;
}
static void save_write(void) {
    FILE *f = fopen("breachline.sav", "wb");
    if (f) { fwrite(&save, sizeof save, 1, f); fclose(f); }
}

/* ------------------------------------------------------------------ */
/* world helpers                                                       */
/* ------------------------------------------------------------------ */
static int tile_blocks(int f, int t, float a, float b) {
    if (t < 0 || t >= W) return 1;
    char c = map[f][t];
    if (c == '#') return 1;
    if (c == 'D' && !door_open[f][t]) {
        float d0 = t * TILE + 6.0f, d1 = t * TILE + 10.0f;
        return b >= d0 && a < d1;
    }
    return 0;
}
static int blocked(int f, float a, float b) {
    if (a > b) { float t = a; a = b; b = t; }
    for (int t = tile_of(a); t <= tile_of(b); t++)
        if (tile_blocks(f, t, a, b)) return 1;
    return 0;
}

static void room_span(int f, float x, float *x0, float *x1) {
    int t = tile_of(x), l = t, r = t;
    if (t >= 0 && t < W && map[f][t] == 'D' && !door_open[f][t]) {
        float dc = t * TILE + 8.0f;
        if (x < dc) { *x1 = dc; r = -1; } else { *x0 = dc; l = -1; }
    }
    if (l >= 0) {
        for (;;) {
            if (l <= 0) { *x0 = 0; break; }
            char c = map[f][l - 1];
            if (c == '#') { *x0 = (float)(l * TILE); break; }
            if (c == 'D' && !door_open[f][l - 1]) { *x0 = (l - 1) * TILE + 8.0f; break; }
            l--;
        }
    }
    if (r >= 0) {
        for (;;) {
            if (r >= W - 1) { *x1 = (float)(W * TILE); break; }
            char c = map[f][r + 1];
            if (c == '#') { *x1 = (float)((r + 1) * TILE); break; }
            if (c == 'D' && !door_open[f][r + 1]) { *x1 = (r + 1) * TILE + 8.0f; break; }
            r++;
        }
    }
}

static float pan_of(float x) { float p = (x - camx - SCREEN_W / 2) / (SCREEN_W / 2); return p < -1 ? -1 : p > 1 ? 1 : p; }
static void sfx_at(int id, float x, int f, float vol) { sfx(id, pan_of(x), f == P.f ? vol : vol * 0.4f); }

static void part(float x, float y, float vx, float vy, float life, float size, uint32_t c, int type, int f) {
    for (int i = 0; i < MAXP; i++) if (PT[i].life <= 0) {
        PT[i] = (Part){ x, y, vx, vy, life, life, size, c, type, f };
        return;
    }
}
static void ftext(float x, float y, const char *s, uint32_t c) {
    int best = 0;
    for (int i = 0; i < MAXFT; i++) { if (FT[i].t <= 0) { best = i; break; } if (FT[i].t < FT[best].t) best = i; }
    FT[best].x = x; FT[best].y = y; FT[best].t = 1.3f; FT[best].c = c;
    snprintf(FT[best].s, sizeof FT[best].s, "%s", s);
}
static void spawn_bullet(float x, float y, int f, float vx, int mine) {
    for (int i = 0; i < MAXB; i++) if (!B[i].on) { B[i] = (Bullet){ x, y, vx, f, 1, mine }; return; }
}
static void alert_room(int f, float x) {
    float a, b;
    room_span(f, x, &a, &b);
    for (int i = 0; i < nE; i++)
        if (E[i].alive && E[i].f == f && E[i].x >= a && E[i].x <= b) E[i].alerted = 1;
}
static void fail(const char *why) {
    if (state != ST_PLAY) return;
    snprintf(fail_msg, sizeof fail_msg, "%s", why);
    state = ST_FAIL; state_t = 0; pause_sel = 0;
    sfx(SFX_FAIL, 0, 0.8f);
    music_set(0);
}

/* ------------------------------------------------------------------ */
/* level setup                                                         */
/* ------------------------------------------------------------------ */
static void load_level(int n) {
    const LevelDef *L = &LEVELS[n];
    cur_level = n;
    memset(map, '#', sizeof map);
    memset(door_open, 0, sizeof door_open);
    memset(door_anim, 0, sizeof door_anim);
    memset(B, 0, sizeof B); memset(PT, 0, sizeof PT); memset(CS, 0, sizeof CS); memset(FT, 0, sizeof FT);
    nE = nH = 0; W = 0;
    for (int f = 0; f < NFLOORS; f++) { int l = (int)strlen(L->rows[f]); if (l > W) W = l; }
    if (W > MAXW) W = MAXW;
    memset(&P, 0, sizeof P);
    P.dir = 1; P.hp = 3; P.ammo = MAG_SIZE;
    for (int f = 0; f < NFLOORS; f++) {
        int len = (int)strlen(L->rows[f]);
        floor_active[f] = 0;
        for (int t = 0; t < W; t++) {
            char c = t < len ? L->rows[f][t] : '#';
            float x = t * TILE + 8.0f;
            switch (c) {
            case 'P': P.x = x; P.f = f; c = '.'; break;
            case 'E': case 'W': case 'B':
                if (nE < MAXE) {
                    Enemy *e = &E[nE++];
                    memset(e, 0, sizeof *e);
                    e->x = x; e->f = f; e->dir = -1; e->alive = 1;
                    e->type = c == 'E' ? ET_GRUNT : c == 'W' ? ET_WALKER : ET_HEAVY;
                    e->hp = e->type == ET_HEAVY ? 3 : 1;
                    e->fire = 0.25f;
                    e->turn_t = 2.0f + (nE % 4) * 0.9f;
                }
                c = '.'; break;
            case 'H':
                if (nH < MAXHOST) H[nH++] = (Hostage){ x, f, 0, 0 };
                c = '.'; break;
            }
            map[f][t] = c;
            if (c != '#') floor_active[f] = 1;
        }
    }
    for (int i = 0; i < nE; i++) {
        float a, b;
        room_span(E[i].f, E[i].x, &a, &b);
        E[i].pmin = a + 14; E[i].pmax = b - 14;
        for (int k = 0; k < nH; k++) {
            if (H[k].f != E[i].f || H[k].x < a || H[k].x > b) continue;
            if (H[k].x > E[i].x) { if (E[i].pmax > H[k].x - 24) E[i].pmax = H[k].x - 24; }
            else if (E[i].pmin < H[k].x + 24) E[i].pmin = H[k].x + 24;
        }
    }
    art_bake();
    shots = hits = damage_taken = kills = breach_kills = score = 0;
    play_t = 0; clear_delay = 0; shake = 0; hitstop = 0; flash_white = 0;
    rng = 0x9E3779B9u ^ (uint32_t)(n * 7919 + 1);
    camx = P.x - SCREEN_W / 2; camy = floor_top(P.f) + FLOOR_H / 2 - VIEW_H / 2;
}

/* ------------------------------------------------------------------ */
/* update                                                              */
/* ------------------------------------------------------------------ */
static void hurt_player(float from_x) {
    if (P.inv > 0 || P.roll > 0 || P.stair_t > 0) return;
    P.hp--; damage_taken++;
    P.inv = 1.0f; P.hurt = 0.4f; shake = 8;
    int g = floor_ground(P.f);
    for (int i = 0; i < 6; i++) part(P.x, g - 24, (P.x > from_x ? 1 : -1) * frand(0.3f, 1.8f), -frand(0.5f, 1.8f), 0.5f, 1, 0xFF8A2020, PT_DROP, P.f);
    sfx(SFX_HURT, 0, 1.0f);
    if (P.hp <= 0) fail("OPERATOR DOWN");
}

static void kill_enemy(Enemy *e, float vx) {
    e->alive = 0; e->deadt = 0; e->dir = vx > 0 ? -1 : 1;
    kills++;
    int bonus = e->stun > 0 || e->breached;
    if (bonus) breach_kills++;
    ftext(e->x, floor_ground(e->f) - 46, bonus ? "BREACH KILL" : "NEUTRALIZED", bonus ? 0xFFFFC040 : 0xFFFFFFFF);
    hitstop = 3;
    sfx_at(SFX_BODY, e->x, e->f, 0.7f);
}

static void update_player(void) {
    int t = tile_of(P.x), g = floor_ground(P.f);
    if (P.stair_t > 0) {
        float before = P.stair_t;
        P.stair_t -= DT;
        if (before > 0.2f && P.stair_t <= 0.2f) P.f = P.stair_to;
        return;
    }
    if (P.reload > 0) { P.reload -= DT; if (P.reload <= 0) P.ammo = MAG_SIZE; }
    if (P.roll_cd > 0) P.roll_cd -= DT;
    if (P.kick > 0) P.kick -= DT;
    if (P.inv > 0) P.inv -= DT;
    if (P.hurt > 0) P.hurt -= DT;
    if (P.shoot_cd > 0) P.shoot_cd -= DT;
    if (P.muzzle > 0) P.muzzle -= DT;
    if (P.recoil > 0) P.recoil -= DT * 6;

    float dx = 0;
    if (P.roll > 0) {
        P.roll -= DT;
        dx = P.dir * 3.3f;
        if (((int)(P.roll * 60)) % 6 == 0) part(P.x - P.dir * 6, g - 2, -P.dir * 0.3f, -0.2f, 0.4f, 3, 0xFF8A8A80, PT_DUST, P.f);
    } else if (P.kick <= 0) {
        float sp = (btn & BTN_SHOOT) ? 1.1f : 1.55f;
        if (btn & BTN_LEFT)  { dx -= sp; P.dir = -1; }
        if (btn & BTN_RIGHT) { dx += sp; P.dir = 1; }
    }
    float target_move = dx != 0 && P.roll <= 0 ? 1.0f : 0.0f;
    P.move += (target_move - P.move) * 0.2f;
    if (dx != 0) {
        float nx = P.x + dx;
        if (!blocked(P.f, nx - 5, nx + 5)) {
            P.x = nx; P.walk += fabsf(dx) * 0.19f;
            P.step_acc += fabsf(dx);
            if (P.step_acc > 17 && P.roll <= 0) { P.step_acc = 0; sfx(SFX_STEP, 0, 0.35f); }
        }
    }

    if (P.roll > 0 || P.kick > 0) return;
    if (map[P.f][t] == 'S') {
        int up = PRESSED(BTN_UP) && P.f > 0 && map[P.f - 1][t] == 'S';
        int dn = PRESSED(BTN_DOWN) && P.f < NFLOORS - 1 && map[P.f + 1][t] == 'S';
        if (up || dn) {
            P.stair_t = 0.4f; P.stair_to = P.f + (up ? -1 : 1); P.x = t * TILE + 8.0f;
            sfx(SFX_STAIRS, 0, 0.6f);
            return;
        }
    }
    if (PRESSED(BTN_ROLL) && P.roll_cd <= 0) { P.roll = 0.36f; P.roll_cd = 0.7f; sfx(SFX_ROLL, 0, 0.7f); }
    if (PRESSED(BTN_RELOAD) && P.reload <= 0 && P.ammo < MAG_SIZE) { P.reload = 1.0f; sfx(SFX_MAGOUT, 0, 0.8f); }

    if ((btn & BTN_SHOOT) && P.shoot_cd <= 0 && P.reload <= 0) {
        if (P.ammo > 0) {
            P.ammo--; shots++;
            P.shoot_cd = 0.16f; P.muzzle = 0.06f; P.recoil = 1;
            float gx = P.x + P.dir * 15, gy = g - 24.5f;
            if (!blocked(P.f, P.x, gx)) spawn_bullet(gx, gy, P.f, P.dir * 11.0f, 1);
            else spawn_bullet(P.x + P.dir * 4, gy, P.f, P.dir * 11.0f, 1);
            for (int i = 0; i < MAXCAS; i++) if (!CS[i].on) {
                CS[i] = (Casing){ P.x + P.dir * 3, gy, -P.dir * frand(0.6f, 1.5f), -frand(1.6f, 2.6f), P.f, 1, 0 };
                break;
            }
            part(gx + P.dir * 2, gy, P.dir * 0.4f, -0.15f, 0.5f, 2.5f, 0xFF9A9A94, PT_SMOKE, P.f);
            alert_room(P.f, P.x);
            if (shake < 1.6f) shake = 1.6f;
            sfx(SFX_SHOT, 0, 0.9f);
        } else if (PRESSED(BTN_SHOOT)) {
            P.reload = 1.0f; sfx(SFX_DRY, 0, 0.8f); sfx(SFX_MAGOUT, 0, 0.8f);
        }
    }

    if (PRESSED(BTN_KICK)) {
        P.kick = 0.28f;
        int done = 0;
        for (int k = 0; k <= 1 && !done; k++) {
            int dt_ = tile_of(P.x) + P.dir * k;
            if (dt_ < 0 || dt_ >= W) continue;
            if (map[P.f][dt_] == 'D' && !door_open[P.f][dt_]) {
                float dc = dt_ * TILE + 8.0f;
                if (fabsf(dc - P.x) < 30 && (dc - P.x) * P.dir > 0) {
                    door_open[P.f][dt_] = 1; door_anim[P.f][dt_] = 0; door_dir[P.f][dt_] = (int8_t)P.dir;
                    shake = 7; flash_white = 0.10f; hitstop = 2;
                    for (int i = 0; i < 18; i++)
                        part(dc, g - frand(6, 54), P.dir * frand(0.6f, 3.4f), -frand(0.3f, 2.6f), frand(1.2f, 2.4f), 1,
                             (i & 1) ? 0xFF7A4A22 : 0xFFB98450, PT_DEBRIS, P.f);
                    for (int i = 0; i < 5; i++) part(dc + P.dir * 6, g - 4, P.dir * frand(0.2f, 1.0f), -frand(0, 0.4f), 0.8f, 4, 0xFF8A8478, PT_DUST, P.f);
                    for (int i = 0; i < nE; i++) {
                        Enemy *e = &E[i];
                        if (!e->alive || e->f != P.f) continue;
                        float rel = (e->x - dc) * P.dir;
                        if (rel > 0 && rel < 66) {
                            e->stun = 1.5f; e->aware = 0; e->breached = 1;
                            float nx = e->x + P.dir * 8;
                            if (!blocked(e->f, nx - 5, nx + 5)) e->x = nx;
                        } else if (rel > 0) e->breached = 1;
                    }
                    alert_room(P.f, P.x);
                    for (int i = 0; i < nE; i++) if (E[i].alive && E[i].f == P.f && E[i].stun <= 0 && E[i].alerted) E[i].aware = 0;
                    sfx(SFX_KICK, P.dir * 0.2f, 1.0f);
                    done = 1;
                }
            }
        }
        for (int i = 0; i < nH && !done; i++)
            if (H[i].state == 0 && H[i].f == P.f && fabsf(H[i].x - P.x) < 20) {
                H[i].state = 1; H[i].t = 0; done = 1;
                ftext(H[i].x, g - 46, "HOSTAGE SECURED", 0xFF5EE08A);
                sfx(SFX_SECURE, 0, 0.8f);
            }
        for (int i = 0; i < nE && !done; i++) {
            Enemy *e = &E[i];
            if (e->alive && e->f == P.f && (e->x - P.x) * P.dir > 0 && fabsf(e->x - P.x) < 22) {
                e->stun = 1.1f; e->aware = 0; shake = 4;
                float nx = e->x + P.dir * 14;
                if (!blocked(e->f, nx - 5, nx + 5)) e->x = nx;
                sfx(SFX_HIT, 0, 0.8f);
                done = 1;
            }
        }
        if (!done) sfx(SFX_ROLL, 0, 0.3f);
    }
}

static void update_enemies(void) {
    for (int i = 0; i < nE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) { e->deadt += DT; continue; }
        if (e->flash > 0) e->flash -= DT;
        if (e->muzzle > 0) e->muzzle -= DT;
        if (e->recoil > 0) e->recoil -= DT * 6;
        if (e->bang > 0) e->bang -= DT;
        e->move *= 0.85f;
        if (e->stun > 0) { e->stun -= DT; continue; }

        int same = e->f == P.f && P.stair_t <= 0;
        float d = P.x - e->x, ad = fabsf(d);
        int los = same && !blocked(e->f, e->x, P.x);
        int facing = (d > 0 && e->dir > 0) || (d < 0 && e->dir < 0);
        int sees = los && ad < 330 && (facing || ad < 44 || e->alerted);

        if (sees) {
            e->dir = d > 0 ? 1 : -1;
            if (!e->aware) {
                e->aware = 1; e->bang = 0.6f;
                e->react = (e->type == ET_HEAVY ? 0.72f : 0.5f) + frand(0, 0.18f);
                sfx_at(SFX_ALERT, e->x, e->f, 0.6f);
            } else if (e->react > 0) {
                e->react -= DT;
            } else {
                e->fire -= DT;
                if (e->fire <= 0) {
                    int g = floor_ground(e->f);
                    spawn_bullet(e->x + e->dir * 15, g - 24.5f, e->f, e->dir * (e->type == ET_HEAVY ? 5.6f : 4.8f), 0);
                    e->muzzle = 0.06f; e->recoil = 1;
                    e->fire = e->type == ET_HEAVY ? 0.5f : 0.95f;
                    for (int k = 0; k < MAXCAS; k++) if (!CS[k].on) {
                        CS[k] = (Casing){ e->x + e->dir * 3, g - 24.5f, -e->dir * frand(0.6f, 1.4f), -frand(1.6f, 2.4f), e->f, 1, 0 };
                        break;
                    }
                    sfx_at(e->type == ET_HEAVY ? SFX_HEAVYSHOT : SFX_ESHOT, e->x, e->f, 0.85f);
                }
            }
        } else {
            e->aware = 0;
            if (e->alerted && same) e->dir = d > 0 ? 1 : -1;
            else if (e->type == ET_WALKER && !e->alerted) {
                float nx = e->x + e->dir * 0.55f;
                if (nx < e->pmin || nx > e->pmax || blocked(e->f, nx - 5, nx + 5)) e->dir = -e->dir;
                else { e->x = nx; e->walk += 0.55f * 0.19f; e->move = 1; }
            } else if (!e->alerted) {
                e->turn_t -= DT;
                if (e->turn_t <= 0) { e->dir = -e->dir; e->turn_t = frand(2.6f, 5.0f); }
            }
        }
    }
}

static void update_bullets(void) {
    for (int i = 0; i < MAXB; i++) {
        Bullet *b = &B[i];
        if (!b->on) continue;
        int steps = (int)ceilf(fabsf(b->vx) / 3.0f);
        for (int s = 0; s < steps && b->on; s++) {
            b->x += b->vx / steps;
            if (blocked(b->f, b->x, b->x)) {
                int t = tile_of(b->x);
                int door = t >= 0 && t < W && map[b->f][t] == 'D';
                float ix = b->x - (b->vx > 0 ? 1 : -1);
                for (int k = 0; k < 5; k++) part(ix, b->y, -(b->vx > 0 ? 1 : -1) * frand(0.5f, 2.2f), frand(-1.6f, 0.4f), 0.35f, 1,
                                                 door ? 0xFFB98450 : 0xFFFFE08A, door ? PT_DEBRIS : PT_SPARK, b->f);
                part(ix, b->y, 0, -0.15f, 0.5f, 2.5f, door ? 0xFF8A6A4A : 0xFF8A8A84, PT_DUST, b->f);
                if (!door && t >= 0 && t < W) {
                    float face = b->vx > 0 ? t * TILE : (t + 1) * TILE - 1;
                    art_add_bullet_hole(b->f, face, (int)b->y + (int)frand(-2, 2));
                }
                sfx_at(SFX_HIT, b->x, b->f, 0.25f);
                b->on = 0; break;
            }
            if (b->mine) {
                for (int k = 0; k < nE; k++) {
                    Enemy *e = &E[k];
                    if (e->alive && e->f == b->f && fabsf(e->x - b->x) < 6) {
                        b->on = 0; hits++;
                        e->hp--; e->flash = 0.08f; e->alerted = 1;
                        for (int q = 0; q < 6; q++) part(e->x, b->y + frand(-3, 3), (b->vx > 0 ? 1 : -1) * frand(0.4f, 2.0f), -frand(0.2f, 1.6f), 0.5f, 1, 0xFF7A1A1A, PT_DROP, e->f);
                        sfx_at(SFX_HIT, e->x, e->f, 0.8f);
                        if (e->hp <= 0) kill_enemy(e, b->vx);
                        else { e->stun = 0; e->bang = 0.3f; }
                        break;
                    }
                }
                for (int k = 0; k < nH && b->on; k++) {
                    Hostage *h = &H[k];
                    if (h->state == 0 && h->f == b->f && fabsf(h->x - b->x) < 5) {
                        b->on = 0; h->state = 2;
                        sfx_at(SFX_HIT, h->x, h->f, 0.8f);
                        fail("HOSTAGE HIT");
                    }
                }
            } else if (b->f == P.f && fabsf(P.x - b->x) < 5 && P.roll <= 0 && P.inv <= 0 && P.stair_t <= 0) {
                b->on = 0; hurt_player(b->x - b->vx);
            }
        }
        if (b->x < -64 || b->x > W * TILE + 64) b->on = 0;
    }
}

static void update_fx(void) {
    for (int i = 0; i < MAXP; i++) {
        Part *p = &PT[i];
        if (p->life <= 0) continue;
        p->life -= DT;
        p->x += p->vx; p->y += p->vy;
        float g = (float)floor_ground(p->f);
        switch (p->type) {
        case PT_SPARK: p->vy += 0.12f; p->vx *= 0.94f; break;
        case PT_DEBRIS: case PT_DROP:
            p->vy += 0.16f;
            if (p->y >= g - 1) { p->y = g - 1; p->vy *= -0.3f; p->vx *= 0.55f; if (fabsf(p->vy) < 0.3f) p->vy = 0; }
            break;
        case PT_DUST: p->vx *= 0.94f; p->vy *= 0.94f; p->size += 0.05f; break;
        case PT_SMOKE: p->vy -= 0.01f; p->vx *= 0.97f; p->size += 0.06f; break;
        }
    }
    for (int i = 0; i < MAXCAS; i++) {
        Casing *c = &CS[i];
        if (!c->on || c->rest) continue;
        c->vy += 0.18f; c->x += c->vx; c->y += c->vy;
        float g = (float)floor_ground(c->f);
        if (c->y >= g - 1) {
            c->y = g - 1;
            if (fabsf(c->vy) > 0.8f && c->f == P.f) sfx_at(SFX_CASING, c->x, c->f, 0.22f);
            c->vy *= -0.35f; c->vx *= 0.6f;
            if (fabsf(c->vy) < 0.4f) { c->vy = 0; if (fabsf(c->vx) < 0.1f) c->rest = 1; }
        }
    }
    for (int i = 0; i < MAXFT; i++) if (FT[i].t > 0) { FT[i].t -= DT; FT[i].y -= 0.35f; }
    for (int f = 0; f < NFLOORS; f++)
        for (int t = 0; t < W; t++)
            if (door_open[f][t] && door_anim[f][t] < 1.6f) door_anim[f][t] += DT * 6;
    for (int i = 0; i < nH; i++) if (H[i].state == 1) H[i].t += DT;
    if (shake > 0) { shake -= 0.5f; if (shake < 0) shake = 0; }
    if (flash_white > 0) flash_white -= DT;
}

static void update_camera(int snap) {
    float bw = (float)(W * TILE);
    float tx = P.x - SCREEN_W / 2 + P.dir * 46;
    float lo = -60, hi = bw + 60 - SCREEN_W;
    if (hi < lo) tx = (bw - SCREEN_W) / 2;
    else { if (tx < lo) tx = lo; if (tx > hi) tx = hi; }
    int f = P.stair_t > 0 ? P.stair_to : P.f;
    float ty = floor_top(f) + FLOOR_H / 2 - VIEW_H / 2 + 4;
    if (ty < -70) ty = -70;
    if (ty > CACHE_H - VIEW_H) ty = (float)(CACHE_H - VIEW_H);
    if (snap) { camx = tx; camy = ty; }
    else { camx += (tx - camx) * 0.11f; camy += (ty - camy) * 0.10f; }
}

static int enemies_left(void) { int n = 0; for (int i = 0; i < nE; i++) n += E[i].alive; return n; }
static int hostages_safe(void) { int n = 0; for (int i = 0; i < nH; i++) n += H[i].state == 1; return n; }

static void finish_level(void) {
    int acc = shots ? hits * 100 / shots : 0;
    score = kills * 100 + breach_kills * 50 + nH * 300 + (damage_taken == 0 ? 500 : 0) + acc * 5;
    int tb = 1000 - (int)(play_t * 8); if (tb > 0) score += tb;
    stars_now = (damage_taken == 0 && acc >= 60) ? 3 : damage_taken <= 1 ? 2 : 1;
    if (stars_now > save.stars[cur_level]) save.stars[cur_level] = (uint8_t)stars_now;
    if (score > save.best[cur_level]) save.best[cur_level] = score;
    if (save.unlocked < cur_level + 2 && cur_level + 1 < NLEVELS) save.unlocked = (uint8_t)(cur_level + 2);
    save_write();
    state = cur_level == NLEVELS - 1 ? ST_WIN : ST_CLEAR;
    state_t = 0; stars_shown = 0; pause_sel = 0;
    sfx(SFX_CLEAR, 0, 0.8f);
    music_set(0);
}

static void begin_brief(int n) { load_level(n); state = ST_BRIEF; state_t = 0; update_camera(1); }

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */
void game_render_loading(uint32_t *fb) {
    SCR.px = fb; SCR.w = SCREEN_W; SCR.h = SCREEN_H;
    tg_set(&SCR, 0, 0);
    vgrad(0, 0, SCREEN_W, SCREEN_H, 0xFF0B0E16, 0xFF1A1E2C);
    text_grad(SCREEN_W / 2 - text_w("BREACHLINE", 4) / 2, 100, "BREACHLINE", 0xFFFFC070, 0xFFE2501A, 4);
    text_c(SCREEN_W / 2, 150, "LOADING...", 0xFFC8CDD6, 1);
}

void game_init(void) {
    save_load();
    state = ST_TITLE; state_t = 0; menu_sel = 0;
    btn = prev_btn = 0;
    load_level(0);
    update_camera(1);
    music_set(1);
}
int game_wants_quit(void) { return quit_req; }
int game_level_count(void) { return NLEVELS; }
int game_debug_state(void) { return state; }
void game_debug_start_level(int n) { begin_brief(n); state = ST_PLAY; music_set(2); }

int game_debug_validate(void) {
    int bad = 0;
    for (int n = 0; n < NLEVELS; n++) {
        const LevelDef *L = &LEVELS[n];
        int players = 0;
        for (int f = 0; f < NFLOORS; f++) {
            const char *r = L->rows[f];
            int len = (int)strlen(r);
            if (len > MAXW) { printf("level %d floor %d too wide (%d)\n", n, f, len); bad++; }
            for (int t = 0; t < len; t++) {
                if (r[t] == 'P') players++;
                if (r[t] == 'S') {
                    int up = f > 0 && (int)strlen(L->rows[f - 1]) > t && L->rows[f - 1][t] == 'S';
                    int dn = f < NFLOORS - 1 && (int)strlen(L->rows[f + 1]) > t && L->rows[f + 1][t] == 'S';
                    if (!up && !dn) { printf("level %d floor %d col %d: stairs lead nowhere\n", n, f, t); bad++; }
                    if ((t > 0 && r[t - 1] == 'D') || (t + 1 < len && r[t + 1] == 'D')) { printf("level %d floor %d col %d: door touches stairs\n", n, f, t); bad++; }
                }
            }
            if (len > 1 && (r[0] != '#' || r[len - 1] != '#')) { printf("level %d floor %d not walled\n", n, f); bad++; }
        }
        if (players != 1) { printf("level %d has %d player starts\n", n, players); bad++; }
    }
    return bad;
}

void game_update(unsigned buttons) {
    prev_btn = btn; btn = buttons;
    state_t += DT; gtime += DT;
    int up = PRESSED(BTN_UP), dn = PRESSED(BTN_DOWN), ok = PRESSED(BTN_SHOOT) || PRESSED(BTN_START), back = PRESSED(BTN_ROLL);
    switch (state) {
    case ST_TITLE:
        if (up) { menu_sel = (menu_sel + 2) % 3; sfx(SFX_UI, 0, 0.6f); }
        if (dn) { menu_sel = (menu_sel + 1) % 3; sfx(SFX_UI, 0, 0.6f); }
        if (ok && state_t > 0.3f) {
            sfx(SFX_UI_OK, 0, 0.7f);
            if (menu_sel == 0) { int n = save.unlocked - 1; if (n >= NLEVELS) n = NLEVELS - 1; if (n < 0) n = 0; begin_brief(n); }
            else if (menu_sel == 1) { state = ST_SELECT; state_t = 0; select_sel = 0; }
            else { state = ST_CONTROLS; state_t = 0; }
        }
        break;
    case ST_SELECT:
        if (up) { select_sel = (select_sel + NLEVELS - 1) % NLEVELS; sfx(SFX_UI, 0, 0.6f); }
        if (dn) { select_sel = (select_sel + 1) % NLEVELS; sfx(SFX_UI, 0, 0.6f); }
        if (back) { state = ST_TITLE; state_t = 0; sfx(SFX_UI, 0, 0.6f); }
        if (ok && state_t > 0.2f) {
            if (select_sel < save.unlocked) { sfx(SFX_UI_OK, 0, 0.7f); begin_brief(select_sel); }
            else sfx(SFX_DRY, 0, 0.7f);
        }
        break;
    case ST_CONTROLS:
        if (back || ok) { state = ST_TITLE; state_t = 0; sfx(SFX_UI, 0, 0.6f); }
        break;
    case ST_BRIEF:
        update_fx();
        if (back) { state = ST_TITLE; state_t = 0; music_set(1); sfx(SFX_UI, 0, 0.6f); }
        else if (state_t > 0.4f && ok) { state = ST_PLAY; state_t = 0; sfx(SFX_UI_OK, 0, 0.7f); music_set(2); }
        break;
    case ST_PLAY:
        if (PRESSED(BTN_START)) { state = ST_PAUSE; pause_sel = 0; sfx(SFX_UI, 0, 0.6f); break; }
        if (hitstop > 0) { hitstop--; break; }
        play_t += DT;
        update_player();
        update_enemies();
        update_bullets();
        update_fx();
        update_camera(0);
        if (state == ST_PLAY && enemies_left() == 0 && hostages_safe() == nH) {
            clear_delay += DT;
            if (clear_delay > 1.0f) finish_level();
        }
        break;
    case ST_PAUSE:
        if (up) { pause_sel = (pause_sel + 2) % 3; sfx(SFX_UI, 0, 0.6f); }
        if (dn) { pause_sel = (pause_sel + 1) % 3; sfx(SFX_UI, 0, 0.6f); }
        if (PRESSED(BTN_START) || back) { state = ST_PLAY; break; }
        if (PRESSED(BTN_SHOOT)) {
            sfx(SFX_UI_OK, 0, 0.7f);
            if (pause_sel == 0) state = ST_PLAY;
            else if (pause_sel == 1) begin_brief(cur_level);
            else { state = ST_TITLE; state_t = 0; music_set(1); }
        }
        break;
    case ST_FAIL:
        update_fx();
        if (up || dn) { pause_sel ^= 1; sfx(SFX_UI, 0, 0.6f); }
        if (state_t > 0.6f && ok) {
            sfx(SFX_UI_OK, 0, 0.7f);
            if (pause_sel == 0) begin_brief(cur_level); else { state = ST_TITLE; state_t = 0; music_set(1); }
        }
        break;
    case ST_CLEAR: case ST_WIN: {
        update_fx();
        int want = state_t > 1.3f ? 1 + (int)((state_t - 1.3f) / 0.3f) : 0;
        if (want > stars_now) want = stars_now;
        while (stars_shown < want) { stars_shown++; sfx(SFX_STAR, 0, 0.8f); }
        if (up || dn) { pause_sel ^= 1; sfx(SFX_UI, 0, 0.6f); }
        if (state_t > 1.0f && ok) {
            sfx(SFX_UI_OK, 0, 0.7f);
            if (pause_sel == 1) begin_brief(cur_level);
            else if (state == ST_WIN) { state = ST_TITLE; state_t = 0; music_set(1); }
            else begin_brief(cur_level + 1);
        }
    } break;
    }
}

/* ------------------------------------------------------------------ */
/* rendering                                                           */
/* ------------------------------------------------------------------ */
static uint8_t vignette[SCREEN_H][SCREEN_W];
static int vignette_ready;

static void build_vignette(void) {
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++) {
            float dx = (x - SCREEN_W / 2.0f) / (SCREEN_W / 2.0f), dy = (y - (HUD_H + VIEW_H / 2.0f)) / (VIEW_H / 2.0f);
            float d = dx * dx * 0.7f + dy * dy;
            float k = 1.0f - 0.42f * fmaxf(0, d - 0.35f);
            if (k < 0.45f) k = 0.45f;
            vignette[y][x] = (uint8_t)(k * 255);
        }
    vignette_ready = 1;
}

static void blit_world(void) {
    uint32_t *fb = SCR.px;
    float lx0 = 0, lx1 = 0;
    int pf = P.stair_t > 0.2f ? -1 : P.f;
    if (pf >= 0) room_span(pf, P.x, &lx0, &lx1);
    int cw = CACHE_LIT.w;
    for (int sy = HUD_H; sy < SCREEN_H; sy++) {
        int wy = sy - HUD_H + camyi;
        if (wy < 0 || wy >= CACHE_H) continue;
        int f = (wy - ROOF) / FLOOR_H;
        int interior = wy >= ROOF && f < NFLOORS && floor_active[f] && wy < floor_ground(f) + 3;
        const uint32_t *lit = CACHE_LIT.px + wy * cw, *dark = CACHE_DARK.px + wy * cw;
        int a = 0, b = 0; /* screen columns that use the lit cache */
        if (!interior) { a = 0; b = SCREEN_W; }
        else if (f == pf) { a = (int)lx0 - camxi; b = (int)lx1 - camxi; }
        uint32_t *row = fb + sy * SCREEN_W;
        int alpha = cache_row_alpha[wy];
        for (int x = 0; x < SCREEN_W; x++) {
            int cx = camxi + MARGIN + x;
            if (cx < 0 || cx >= cw) continue;
            uint32_t c = (x >= a && x < b) ? lit[cx] : dark[cx];
            if (alpha && !(c >> 24)) continue;
            row[x] = c;
        }
    }
}

static int ent_light(int f, float x) {
    int xi = (int)x; if (xi < 0) xi = 0; if (xi >= W * TILE) xi = W * TILE - 1;
    int l = light_col[f][xi];
    l = 120 + l * 3 / 4;          /* keep characters readable in dim rooms */
    if (l < 200) l = 200; if (l > 330) l = 330;
    return l;
}

static void muzzle_fx(float mx, float my, int dir) {
    glow(mx, my, 34, 0xFFFFB050, 120);
    capsule(mx, my, mx + dir * 9, my, 1.6f, 0xFFFFF4C0);
    capsule(mx, my, mx + dir * 4, my - 4, 0.9f, 0xFFFFE090);
    capsule(mx, my, mx + dir * 4, my + 4, 0.9f, 0xFFFFE090);
    disc(mx, my, 2.4f, 0xFFFFFFFF);
}

static void bubble(int cx, int y, const char *s) {
    int w = text_w(s, 1) + 10;
    fill_a(cx - w / 2, y - 3, w, 13, 0xFF0A0C10, 200);
    fill(cx - w / 2, y - 3, w, 1, 0xFFFF8A2A);
    text_c(cx, y, s, 0xFFFFFFFF, 1);
}

static void draw_entities(void) {
    float lx0 = 0, lx1 = 0;
    int pf = P.stair_t > 0.2f ? -1 : P.f;
    if (pf >= 0) room_span(pf, P.x, &lx0, &lx1);
#define VIS(f_, x_) ((f_) == pf && (x_) >= lx0 - 4 && (x_) <= lx1 + 4)
#define SX(x_) ((x_) - camxi)
#define SY(y_) ((y_) - camyi + HUD_H)

    /* doors */
    for (int f = 0; f < NFLOORS; f++) {
        if (!floor_active[f]) continue;
        int g = floor_ground(f);
        for (int t = 0; t < W; t++) {
            if (map[f][t] != 'D') continue;
            int sx = SX(t * TILE);
            if (sx < -16 || sx > SCREEN_W) continue;
            int vis = f == pf && t * TILE + 16 >= lx0 && t * TILE <= lx1;
            int l = vis ? ent_light(f, t * TILE + 8) : 46;
            float o = door_open[f][t] ? door_anim[f][t] : 0;
            float ov = o > 1 ? 1 + sinf((o - 1) * 10) * 0.12f * (1.6f - o) : o;
            draw_door(sx, SY(g), ov, door_dir[f][t], l, 0);
        }
    }
    /* casings */
    for (int i = 0; i < MAXCAS; i++) {
        Casing *c = &CS[i];
        if (!c->on || !VIS(c->f, c->x)) continue;
        fill(SX((int)c->x), SY((int)c->y), 2, 1, 0xFFD8A848);
    }
    /* bodies */
    for (int i = 0; i < nE; i++) {
        Enemy *e = &E[i];
        if (e->alive || !VIS(e->f, e->x)) continue;
        int kind = e->type == ET_HEAVY ? 2 : 1;
        int g = floor_ground(e->f), l = ent_light(e->f, e->x) * 3 / 4;
        if (e->deadt < 0.08f) draw_sprite(kind, SF_SHOOT, SX(e->x), SY(g), -e->dir, 256, 0xFFFFFFFF, 1, NULL, NULL);
        else if (e->deadt < 0.2f) draw_sprite(kind, SF_STUN0, SX(e->x), SY(g) + 3, -e->dir, l, 0, 1, NULL, NULL);
        else draw_sprite(kind, SF_DEAD, SX(e->x) - e->dir * 6, SY(g), -e->dir, l, 0, 1, NULL, NULL);
    }
    /* hostages */
    for (int i = 0; i < nH; i++) {
        Hostage *h = &H[i];
        if (!VIS(h->f, h->x) || h->state == 2) continue;
        int g = floor_ground(h->f);
        int fr = h->state == 1 ? SF_STAND : (((int)(gtime * 9 + i)) & 1) ? SF_KNEEL1 : SF_KNEEL0;
        draw_sprite(3, fr, SX(h->x), SY(g), h->x < P.x ? 1 : -1, ent_light(h->f, h->x), 0, 1, NULL, NULL);
        if (h->state == 1) {
            int bx = SX((int)h->x), by = SY(g - 52) + (int)(sinf(gtime * 3) * 1.5f);
            disc(bx, by, 5, 0xFF1F8A4C); ring(bx, by, 5, 0xFF0A2A16);
            line_a(bx - 2.5f, by, bx - 0.5f, by + 2, 0xFFFFFFFF, 256); line_a(bx - 0.5f, by + 2, bx + 2.5f, by - 2, 0xFFFFFFFF, 256);
        }
    }
    /* guards */
    for (int i = 0; i < nE; i++) {
        Enemy *e = &E[i];
        if (!e->alive || !VIS(e->f, e->x)) continue;
        int g = floor_ground(e->f);
        int kind = e->type == ET_HEAVY ? 2 : 1, ready = e->aware || e->alerted;
        int wf = ((int)(e->walk / 1.5708f)) & 3, fr;
        if (e->stun > 0) fr = ((int)(gtime * 6)) & 1 ? SF_STUN1 : SF_STUN0;
        else if (e->recoil > 0.4f) fr = SF_SHOOT;
        else if (e->move > 0.3f) fr = (ready ? SF_WALK0 : SF_LWALK0) + wf;
        else fr = (ready ? SF_IDLE0 : SF_LOW0) + (((int)(gtime * 1.5f + i)) & 1);
        float mx, my;
        draw_sprite(kind, fr, SX(e->x), SY(g), e->dir, ent_light(e->f, e->x), e->flash > 0 ? 0xFFFFFFFF : 0, 1, &mx, &my);
        if (e->muzzle > 0) muzzle_fx(mx, my, e->dir);
        int hx = SX((int)e->x), hy = SY(g - 56);
        if (e->stun > 0) {
            for (int k = 0; k < 3; k++) {
                float a = gtime * 6 + k * 2.1f;
                disc(hx + cosf(a) * 7, hy + 6 + sinf(a) * 2, 1.3f, 0xFFFFE070);
            }
        } else if (e->aware && e->react > 0) {
            fill(hx - 1, hy - 4, 3, 7, 0xFFFF4040); fill(hx - 1, hy + 4, 3, 2, 0xFFFF4040);
            float tot = e->type == ET_HEAVY ? 0.8f : 0.6f;
            fill(hx - 8, hy + 8, 16, 2, 0xFF200808);
            fill(hx - 8, hy + 8, (int)(16 * (1 - e->react / tot)), 2, 0xFFFF4040);
        }
        if (e->type == ET_HEAVY)
            for (int k = 0; k < 3; k++) fill(hx - 7 + k * 5, hy + 13, 4, 2, k < e->hp ? 0xFFE53B3B : 0xFF3A1010);
    }
    /* player */
    if (P.stair_t <= 0.0f) {
        int g = floor_ground(P.f);
        int blink = P.inv > 0 && ((int)(P.inv * 20) & 1);
        if (!blink) {
            int fr;
            if (P.roll > 0) fr = SF_ROLL0 + (((int)((1 - P.roll / 0.36f) * 8)) & 3);
            else if (P.kick > 0) fr = SF_KICK;
            else if (P.recoil > 0.45f) fr = SF_SHOOT;
            else if (P.move > 0.3f) fr = SF_WALK0 + (((int)(P.walk / 1.5708f)) & 3);
            else fr = SF_IDLE0 + (((int)(gtime * 1.4f)) & 1);
            float mx, my;
            draw_sprite(0, fr, SX(P.x), SY(g), P.dir, ent_light(P.f, P.x) + 10,
                        P.hurt > 0.3f ? 0xFFFF7070 : 0, 1, &mx, &my);
            if (P.muzzle > 0 && P.roll <= 0) muzzle_fx(mx, my, P.dir);
            if (P.reload > 0) {
                int x = SX((int)P.x), y = SY(g - 58);
                fill(x - 11, y, 22, 4, 0xFF0A0C10);
                fill(x - 10, y + 1, (int)(20 * (1 - P.reload)), 2, 0xFF5EE08A);
            }
        }
    }
    /* bullets */
    for (int i = 0; i < MAXB; i++) {
        Bullet *b = &B[i];
        if (!b->on || b->f != pf) continue;
        float x = SX(b->x), y = SY(b->y);
        float tl = b->mine ? 26 : 16;
        uint32_t c = b->mine ? 0xFFFFE8A0 : 0xFFFF7050;
        line_add(x - (b->vx > 0 ? tl : -tl), y, x, y, c, 255);
        disc(x, y, 1.1f, 0xFFFFFFFF);
        glow(x, y, 8, c, 70);
    }
    /* particles */
    for (int i = 0; i < MAXP; i++) {
        Part *p = &PT[i];
        if (p->life <= 0 || p->f != pf) continue;
        float x = SX(p->x), y = SY(p->y), k = p->life / p->max;
        switch (p->type) {
        case PT_SPARK: line_add(x, y, x - p->vx * 2, y - p->vy * 2, p->c, (int)(255 * k)); break;
        case PT_DEBRIS: fill((int)x, (int)y, 2, 1, p->c); break;
        case PT_DROP: fill((int)x, (int)y, 1, 1, p->c); break;
        case PT_DUST: disc_a(x, y, p->size, p->c, (int)(110 * k)); break;
        case PT_SMOKE: disc_a(x, y, p->size, p->c, (int)(90 * k)); break;
        }
    }
    /* floating text */
    for (int i = 0; i < MAXFT; i++) {
        if (FT[i].t <= 0) continue;
        int a = FT[i].t > 0.3f ? 256 : (int)(FT[i].t / 0.3f * 256);
        if (a < 60) continue;
        text_c(SX((int)FT[i].x), SY((int)FT[i].y), FT[i].s, cmul(FT[i].c, a), 1);
    }
    /* context prompt */
    if (state == ST_PLAY && P.stair_t <= 0 && P.roll <= 0) {
        int g = floor_ground(P.f), t = tile_of(P.x);
        const char *prompt = NULL;
        for (int k = 0; k <= 1; k++) {
            int dt_ = t + P.dir * k;
            if (dt_ >= 0 && dt_ < W && map[P.f][dt_] == 'D' && !door_open[P.f][dt_] &&
                fabsf(dt_ * TILE + 8 - P.x) < 30 && (dt_ * TILE + 8 - P.x) * P.dir > 0) prompt = "\1 BREACH";
        }
        for (int i = 0; i < nH; i++)
            if (H[i].state == 0 && H[i].f == P.f && fabsf(H[i].x - P.x) < 20) prompt = "\1 SECURE";
        if (!prompt && map[P.f][t] == 'S') {
            int up = P.f > 0 && map[P.f - 1][t] == 'S', dn = P.f < NFLOORS - 1 && map[P.f + 1][t] == 'S';
            prompt = up && dn ? "UP / DOWN" : up ? "UP  STAIRS" : dn ? "DOWN  STAIRS" : NULL;
        }
        if (!prompt && P.ammo == 0 && P.reload <= 0) prompt = "\4 RELOAD";
        if (prompt) bubble(SX((int)P.x), SY(g - 66), prompt);
    }
#undef VIS
#undef SX
#undef SY
}

static void apply_vignette(void) {
    if (!vignette_ready) build_vignette();
    for (int y = HUD_H; y < SCREEN_H; y++) {
        uint32_t *row = SCR.px + y * SCREEN_W;
        const uint8_t *v = vignette[y];
        for (int x = 0; x < SCREEN_W; x++) {
            int k = v[x];
            if (k >= 250) continue;
            uint32_t p = row[x];
            uint32_t rb = (((p & 0xFF00FF) * k) >> 8) & 0xFF00FF, g = (((p & 0x00FF00) * k) >> 8) & 0x00FF00;
            row[x] = 0xFF000000 | rb | g;
        }
    }
}

static void icon_target(int x, int y, uint32_t c) {
    ring(x, y, 4, c); fill(x - 6, y, 3, 1, c); fill(x + 4, y, 3, 1, c); fill(x, y - 6, 1, 3, c); fill(x, y + 4, 1, 3, c);
}
static void icon_person(int x, int y, uint32_t c) { disc(x, y - 3, 2, c); capsule(x, y + 0.5f, x, y + 4, 2.2f, c); }

static void draw_hud(void) {
    char buf[48];
    tg_set(&SCR, 0, 0);
    vgrad(0, 0, SCREEN_W, HUD_H, 0xFF1A1E26, 0xFF0C0E12);
    fill(0, HUD_H - 1, SCREEN_W, 1, 0xFFFF8A2A);
    fill(0, HUD_H - 2, SCREEN_W, 1, 0xFF3A2410);
    for (int i = 0; i < 3; i++) {
        int x = 6 + i * 15;
        if (i < P.hp) { fill(x, 6, 12, 10, 0xFF3FBF6A); fill(x, 6, 12, 2, 0xFF8AF0A8); fill(x, 14, 12, 2, 0xFF2A8A4A); }
        else { fill(x, 6, 12, 10, 0xFF2A1416); fill(x + 1, 7, 10, 8, 0xFF140A0B); }
    }
    for (int i = 0; i < MAG_SIZE; i++) {
        int x = 56 + i * 6;
        if (P.reload > 0 || i >= P.ammo) { fill(x, 5, 3, 12, 0xFF262A32); continue; }
        fill(x, 8, 3, 9, 0xFFD8A848); fill(x, 8, 1, 9, 0xFFF8D888); fill(x, 5, 3, 3, 0xFFB0B4BA);
    }
    if (P.reload > 0) {
        fill(56, 18, 46, 2, 0xFF1A1E24);
        fill(56, 18, (int)(46 * (1 - P.reload)), 2, 0xFF5EE08A);
        text(108, 7, "RELOAD", 0xFF5EE08A, 1);
    }
    text_c(SCREEN_W / 2, 4, LEVELS[cur_level].name, 0xFFFFFFFF, 1);
    {
        int na = 0; for (int f = 0; f < NFLOORS; f++) na += floor_active[f];
        int x = SCREEN_W / 2 - na * 5, k = 0;
        for (int f = 0; f < NFLOORS; f++) {
            if (!floor_active[f]) continue;
            fill(x + k * 10, 14, 8, 3, f == P.f ? 0xFFFF8A2A : 0xFF3A3E46);
            k++;
        }
    }
    int rx = SCREEN_W - 8;
    snprintf(buf, sizeof buf, "%02d:%02d", (int)play_t / 60, (int)play_t % 60);
    rx -= text_w(buf, 1); text(rx, 7, buf, 0xFFC8CDD6, 1);
    snprintf(buf, sizeof buf, "%d/%d", hostages_safe(), nH);
    rx -= text_w(buf, 1) + 14; text(rx, 7, buf, 0xFFF2C94C, 1); icon_person(rx - 7, 10, 0xFFF2C94C);
    snprintf(buf, sizeof buf, "%d", enemies_left());
    rx -= text_w(buf, 1) + 22; text(rx, 7, buf, 0xFFFF6A5A, 1); icon_target(rx - 9, 10, 0xFFFF6A5A);
}

static void panel(int x, int y, int w, int h) {
    fill_a(x, y, w, h, 0xFF0C0E13, 225);
    fill(x, y, w, 2, 0xFFFF8A2A);
    fill_a(x, y + h - 1, w, 1, 0xFFFFFFFF, 30);
    fill_a(x, y, 1, h, 0xFFFFFFFF, 30); fill_a(x + w - 1, y, 1, h, 0xFFFFFFFF, 30);
    fill(x - 2, y - 2, 6, 1, 0xFFFF8A2A); fill(x - 2, y - 2, 1, 6, 0xFFFF8A2A);
    fill(x + w - 4, y + h + 1, 6, 1, 0xFFFF8A2A); fill(x + w + 1, y + h - 4, 1, 6, 0xFFFF8A2A);
}

static void menu_item(int x, int y, const char *s, int sel, int enabled) {
    if (sel) {
        fill_a(x - 8, y - 4, 150, 15, 0xFFFF8A2A, 60);
        fill(x - 8, y - 4, 3, 15, 0xFFFF8A2A);
        text(x + (int)(sinf(gtime * 8) * 1.2f + 1), y, s, 0xFFFFFFFF, 1);
    } else text(x, y, s, enabled ? 0xFF9AA0AA : 0xFF4A4E56, 1);
}

static void star_icon(float cx, float cy, float r, uint32_t c) {
    float vx[10], vy[10];
    for (int k = 0; k < 10; k++) {
        float a = -1.5708f + k * 0.6283f, rr = (k & 1) ? r * 0.45f : r;
        vx[k] = cx + cosf(a) * rr; vy[k] = cy + sinf(a) * rr;
    }
    for (int y = (int)(cy - r); y <= (int)(cy + r); y++)
        for (int x = (int)(cx - r); x <= (int)(cx + r); x++) {
            float px = x + 0.5f, py = y + 0.5f;
            int in = 0;
            for (int i = 0, j = 9; i < 10; j = i++)
                if (((vy[i] > py) != (vy[j] > py)) && (px < (vx[j] - vx[i]) * (py - vy[i]) / (vy[j] - vy[i]) + vx[i])) in = !in;
            if (in) px_set(x, y, py < cy ? cmul(c, 290) : c);
        }
}

static void draw_title(void) {
    float cx = gtime * 14;
    art_sky(&SCR, cx, 40, gtime);
    tg_set(&SCR, 0, 0);
    /* rooftop foreground */
    vgrad(0, 206, SCREEN_W, 66, 0xFF15171C, 0xFF0A0B0E);
    fill(0, 204, SCREEN_W, 3, 0xFF3A3E46);
    fill(330, 176, 60, 30, 0xFF121418); fill(334, 180, 52, 2, 0xFF1E2228);
    fill(40, 150, 3, 56, 0xFF121418); fill(30, 162, 22, 2, 0xFF121418);
    disc(41.5f, 148, 1.6f, 0xFFFF3030); glow(41.5f, 148, 10, 0xFFFF2020, (int)(100 + 80 * sinf(gtime * 3)));
    glow(130, 160, 60, 0xFF2FD3C6, 22);
    draw_sprite(0, SF_LOW0 + (((int)(gtime * 1.3f + 1)) & 1), 66, 206, 1, 150, 0, 3, NULL, NULL);
    draw_sprite(0, SF_IDLE0 + (((int)(gtime * 1.3f)) & 1), 150, 206, 1, 245, 0, 3, NULL, NULL);
    /* rain */
    for (int i = 0; i < 90; i++) {
        uint32_t h = hash32(i * 31 + 7);
        float x = fmodf((h % 600) + gtime * 60 + i * 3, 600) - 60, y = fmodf((h >> 10) % 300 + gtime * 380, 300) - 20;
        line_a(x, y, x - 3, y + 9, 0xFF8AA0C8, 70);
    }
    /* logo */
    glow(SCREEN_W / 2, 50, 110, 0xFFFF7A1A, 46);
    text_grad(SCREEN_W / 2 - text_w("BREACHLINE", 5) / 2, 28, "BREACHLINE", 0xFFFFC070, 0xFFE2501A, 5);
    {
        const char *sub = "C L O S E   Q U A R T E R S   R E S C U E";
        text_c(SCREEN_W / 2, 72, sub, 0xFFC8CDD6, 1);
        fill(SCREEN_W / 2 - 150, 84, 300, 1, 0xFF3A2410);
    }
    static const char *items[3] = { "START OPERATION", "OPERATIONS", "CONTROLS" };
    panel(296, 112, 160, 72);
    for (int i = 0; i < 3; i++) menu_item(312, 124 + i * 18, items[i], menu_sel == i, 1);
    text(8, SCREEN_H - 12, "\2 SELECT   UP/DOWN MOVE", 0xFF8A909A, 1);
    text(SCREEN_W - text_w("V2.0", 1) - 6, SCREEN_H - 12, "V2.0", 0xFF5A606A, 1);
}

static void draw_select(void) {
    char buf[48];
    art_sky(&SCR, gtime * 8, 40, gtime);
    tg_set(&SCR, 0, 0);
    fill_a(0, 0, SCREEN_W, SCREEN_H, 0xFF05060A, 120);
    text(20, 14, "OPERATIONS", 0xFFFF8A2A, 2);
    panel(16, 40, 196, 210);
    for (int i = 0; i < NLEVELS; i++) {
        int y = 52 + i * 38, locked = i >= save.unlocked;
        if (i == select_sel) { fill_a(20, y - 4, 188, 32, 0xFFFF8A2A, 50); fill(20, y - 4, 3, 32, 0xFFFF8A2A); }
        snprintf(buf, sizeof buf, "%d", i + 1);
        text(30, y + 2, buf, locked ? 0xFF4A4E56 : 0xFFFF8A2A, 2);
        text(52, y, locked ? "LOCKED" : LEVELS[i].name, locked ? 0xFF4A4E56 : 0xFFFFFFFF, 1);
        for (int s = 0; s < 3; s++) star_icon(56 + s * 12, y + 17, 4.5f, s < save.stars[i] ? 0xFFFFD040 : 0xFF2A2E36);
    }
    panel(224, 40, 240, 210);
    int i = select_sel;
    if (i < save.unlocked) {
        text(236, 52, LEVELS[i].name, 0xFFFFFFFF, 2);
        int w = 0; for (int f = 0; f < NFLOORS; f++) { int l = (int)strlen(LEVELS[i].rows[f]); if (l > w) w = l; }
        int cell = w > 0 ? 216 / w : 3; if (cell > 4) cell = 4; if (cell < 2) cell = 2;
        draw_minimap(i, 236, 80, cell);
        text(236, 168, LEVELS[i].brief1, 0xFFC8CDD6, 1);
        snprintf(buf, sizeof buf, "BEST SCORE  %d", (int)save.best[i]);
        text(236, 200, buf, 0xFFF2C94C, 1);
        text(236, 228, "\2 BRIEFING   \3 BACK", 0xFF8A909A, 1);
    } else {
        text_c(344, 130, "CLEAR THE PREVIOUS", 0xFF6A707A, 1);
        text_c(344, 144, "OPERATION TO UNLOCK", 0xFF6A707A, 1);
    }
}

static void draw_controls(void) {
    art_sky(&SCR, gtime * 8, 40, gtime);
    tg_set(&SCR, 0, 0);
    fill_a(0, 0, SCREEN_W, SCREEN_H, 0xFF05060A, 120);
    text(20, 14, "CONTROLS", 0xFFFF8A2A, 2);
    panel(40, 44, 400, 196);
    static const char *L[][2] = {
        { "D-PAD / STICK", "MOVE" }, { "UP / DOWN", "USE STAIRS" }, { "\2", "SHOOT (HOLD FOR AUTO)" },
        { "\1", "BREACH DOOR / SECURE HOSTAGE / SHOVE" }, { "\3", "COMBAT ROLL - DODGES BULLETS" },
        { "\4", "RELOAD" }, { "START", "PAUSE" },
    };
    for (int i = 0; i < 7; i++) {
        text(60, 60 + i * 20, L[i][0], 0xFFFFFFFF, i >= 2 && i <= 5 ? 2 : 1);
        text(170, 62 + i * 20, L[i][1], 0xFFC8CDD6, 1);
    }
    text_c(SCREEN_W / 2, 222, "TIP: KICKED DOORS STUN GUARDS BEHIND THEM", 0xFFF2C94C, 1);
}

void game_render(uint32_t *fb) {
    char buf[64];
    SCR.px = fb; SCR.w = SCREEN_W; SCR.h = SCREEN_H;
    if (state == ST_TITLE) { draw_title(); return; }
    if (state == ST_SELECT) { draw_select(); return; }
    if (state == ST_CONTROLS) { draw_controls(); return; }

    int sx = 0, sy = 0;
    if (shake > 0.5f) { sx = (int)frand(-shake, shake) / 2; sy = (int)frand(-shake, shake) / 2; }
    camxi = (int)floorf(camx) + sx; camyi = (int)floorf(camy) + sy;

    int sky_rows = 0;
    for (int y = HUD_H; y < SCREEN_H; y++) {
        int wy = y - HUD_H + camyi;
        if (wy < 0 || (wy < CACHE_H && cache_row_alpha[wy])) sky_rows = y + 1;
    }
    tg_set(&SCR, 0, 0);
    fill(0, HUD_H, SCREEN_W, VIEW_H, 0xFF000000);
    if (sky_rows > HUD_H) {
        art_sky(&SCR, camx, camy, gtime);
    }
    blit_world();
    tg_set(&SCR, 0, 0);
    tg_clip(0, HUD_H, SCREEN_W, SCREEN_H);
    draw_entities();
    if (P.stair_t > 0) {
        float u = P.stair_t / 0.4f;
        int a = (int)(256 * (1 - fabsf(u - 0.5f) * 2));
        fill_a(0, HUD_H, SCREEN_W, VIEW_H, 0xFF000000, a);
    }
    if (flash_white > 0) fill_add(0, HUD_H, SCREEN_W, VIEW_H, 0xFFFFFFFF, (int)(flash_white * 400));
    if (P.hurt > 0) fill_a(0, HUD_H, SCREEN_W, VIEW_H, 0xFFC01010, (int)(P.hurt * 110));
    apply_vignette();
    draw_hud();
    tg_set(&SCR, 0, 0);

    switch (state) {
    case ST_BRIEF: {
        fill_a(0, HUD_H, SCREEN_W, VIEW_H, 0xFF05060A, 110);
        panel(50, 46, 380, 196);
        snprintf(buf, sizeof buf, "OPERATION %d / %d", cur_level + 1, NLEVELS);
        text(66, 56, buf, 0xFFFF8A2A, 1);
        text(66, 70, LEVELS[cur_level].name, 0xFFFFFFFF, 3);
        int w = W, cell = 340 / (w > 0 ? w : 1); if (cell > 5) cell = 5; if (cell < 2) cell = 2;
        draw_minimap(cur_level, 66, 102, cell);
        text(66, 176, LEVELS[cur_level].brief1, 0xFFC8CDD6, 1);
        text(66, 188, LEVELS[cur_level].brief2, 0xFFC8CDD6, 1);
        snprintf(buf, sizeof buf, "OBJECTIVES: NEUTRALIZE %d   RESCUE %d", nE, nH);
        text(66, 206, buf, 0xFFF2C94C, 1);
        if (((int)(state_t * 2)) & 1) text(66, 224, "\2 BREACH", 0xFFFFFFFF, 1);
        text(150, 224, "\3 BACK", 0xFF8A909A, 1);
    } break;
    case ST_PAUSE: {
        fill_a(0, HUD_H, SCREEN_W, VIEW_H, 0xFF05060A, 140);
        panel(150, 80, 180, 110);
        text_c(SCREEN_W / 2, 92, "PAUSED", 0xFFFFFFFF, 2);
        static const char *it[3] = { "RESUME", "RESTART", "QUIT TO TITLE" };
        for (int i = 0; i < 3; i++) menu_item(176, 122 + i * 18, it[i], pause_sel == i, 1);
    } break;
    case ST_FAIL: {
        fill_a(0, HUD_H, SCREEN_W, VIEW_H, 0xFF200406, 120);
        panel(130, 80, 220, 112);
        text_c(SCREEN_W / 2, 92, "MISSION FAILED", 0xFFFF5050, 2);
        text_c(SCREEN_W / 2, 116, fail_msg, 0xFFFFFFFF, 1);
        menu_item(176, 144, "RETRY", pause_sel == 0, 1);
        menu_item(176, 162, "QUIT TO TITLE", pause_sel == 1, 1);
    } break;
    case ST_CLEAR: case ST_WIN: {
        fill_a(0, HUD_H, SCREEN_W, VIEW_H, 0xFF05060A, 130);
        panel(110, 40, 260, 206);
        text_c(SCREEN_W / 2, 52, state == ST_WIN ? "ALL OPERATIONS COMPLETE" : "AREA SECURE", 0xFF5EE08A, 2);
        int acc = shots ? hits * 100 / shots : 0;
        float u = state_t / 1.0f; if (u > 1) u = 1;
        int y = 80;
        snprintf(buf, sizeof buf, "%02d:%02d", (int)play_t / 60, (int)play_t % 60);
        text(130, y, "TIME", 0xFF9AA0AA, 1); text(350 - text_w(buf, 1), y, buf, 0xFFFFFFFF, 1); y += 14;
        snprintf(buf, sizeof buf, "%d%%", (int)(acc * u));
        text(130, y, "ACCURACY", 0xFF9AA0AA, 1); text(350 - text_w(buf, 1), y, buf, 0xFFFFFFFF, 1); y += 14;
        snprintf(buf, sizeof buf, "%d", (int)(kills * u));
        text(130, y, "NEUTRALIZED", 0xFF9AA0AA, 1); text(350 - text_w(buf, 1), y, buf, 0xFFFFFFFF, 1); y += 14;
        snprintf(buf, sizeof buf, "%d", (int)(breach_kills * u));
        text(130, y, "BREACH KILLS", 0xFF9AA0AA, 1); text(350 - text_w(buf, 1), y, buf, 0xFFFFC040, 1); y += 14;
        snprintf(buf, sizeof buf, "%d", damage_taken);
        text(130, y, "HITS TAKEN", 0xFF9AA0AA, 1); text(350 - text_w(buf, 1), y, buf, damage_taken ? 0xFFFF6A5A : 0xFF5EE08A, 1); y += 18;
        fill(130, y - 4, 220, 1, 0xFF2A2E36);
        snprintf(buf, sizeof buf, "%d", (int)(score * u));
        text(130, y + 2, "SCORE", 0xFFFF8A2A, 1); text(350 - text_w(buf, 2), y - 2, buf, 0xFFFFFFFF, 2);
        for (int s = 0; s < 3; s++) {
            float cx = SCREEN_W / 2 - 30 + s * 30, cy = 186;
            if (s < stars_shown) { glow(cx, cy, 18, 0xFFFFC040, 60); star_icon(cx, cy, 10, 0xFFFFD040); }
            else star_icon(cx, cy, 10, 0xFF2A2E36);
        }
        menu_item(150, 210, state == ST_WIN ? "BACK TO TITLE" : "NEXT OPERATION", pause_sel == 0, 1);
        menu_item(150, 226, "RETRY", pause_sel == 1, 1);
    } break;
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* test autopilot (used only by tools/headless to prove levels are     */
/* beatable; not part of the shipped game loop)                        */
/* ------------------------------------------------------------------ */
unsigned game_debug_bot(void) {
    static int tick;
    tick++;
    if (state != ST_PLAY) return (tick & 1) ? BTN_SHOOT : 0;
    unsigned b = 0;
    if (P.stair_t > 0) return 0;
    /* visible enemy: shoot it */
    float best = 1e9f; Enemy *tgt = NULL;
    for (int i = 0; i < nE; i++) {
        Enemy *e = &E[i];
        if (!e->alive || e->f != P.f) continue;
        if (blocked(P.f, P.x, e->x)) continue;
        float d = fabsf(e->x - P.x);
        if (d < best && d < 320) { best = d; tgt = e; }
    }
    if (tgt) {
        int dir = tgt->x > P.x ? 1 : -1;
        if (dir != P.dir) return dir > 0 ? BTN_RIGHT : BTN_LEFT;
        if (P.ammo == 0 && P.reload <= 0) return (tick & 1) ? BTN_RELOAD : 0;
        int inflight = 0;
        for (int i = 0; i < MAXB; i++) if (B[i].on && B[i].mine) inflight = 1;
        if (P.shoot_cd <= 0 && !inflight && (tick & 1)) b |= BTN_SHOOT;
        return b;
    }
    if (P.ammo < 4 && P.reload <= 0) return (tick & 1) ? BTN_RELOAD : 0;
    /* next objective: nearest enemy or unsecured hostage */
    float gx = 0; int gf = -1, ish = 0; best = 1e9f;
    for (int i = 0; i < nE; i++) if (E[i].alive) {
        float d = fabsf(E[i].x - P.x) + abs(E[i].f - P.f) * 2000;
        if (d < best) { best = d; gx = E[i].x; gf = E[i].f; ish = 0; }
    }
    for (int i = 0; i < nH; i++) if (H[i].state == 0) {
        float d = fabsf(H[i].x - P.x) + abs(H[i].f - P.f) * 2000 + 400;
        if (d < best) { best = d; gx = H[i].x; gf = H[i].f; ish = 1; }
    }
    if (gf < 0) return 0;
    if (gf != P.f) {
        int want = gf < P.f ? -1 : 1, st = -1; float sd = 1e9f;
        for (int t = 0; t < W; t++)
            if (map[P.f][t] == 'S' && map[P.f + want][t] == 'S' && fabsf(t * TILE + 8 - P.x) < sd) { sd = fabsf(t * TILE + 8 - P.x); st = t; }
        if (st < 0) return 0;
        gx = st * TILE + 8.0f;
        if (fabsf(gx - P.x) < 3) return (tick & 1) ? (want < 0 ? BTN_UP : BTN_DOWN) : 0;
    } else if (ish && fabsf(gx - P.x) < 16) return (tick & 1) ? BTN_KICK : 0;
    int dir = gx > P.x ? 1 : -1;
    int t2 = tile_of(P.x + dir * 14);
    if (t2 >= 0 && t2 < W && map[P.f][t2] == 'D' && !door_open[P.f][t2]) {
        if (dir != P.dir) return dir > 0 ? BTN_RIGHT : BTN_LEFT;
        return (tick & 1) ? BTN_KICK : 0;
    }
    return dir > 0 ? BTN_RIGHT : BTN_LEFT;
}
int game_debug_info(int *hp, int *left, int *score_out) { *hp = P.hp; *left = enemies_left(); *score_out = score; return state; }
