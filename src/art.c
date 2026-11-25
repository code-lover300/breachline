/* BREACHLINE - environment art (baked per level), sky, character rig, doors.
 * All art is generated in code: no image files. */
#include "internal.h"
#include "sprites_gen.h"

/* ------------------------------------------------------------------ */
/* caches                                                              */
/* ------------------------------------------------------------------ */
static uint32_t lit_buf[CACHE_MAXW * CACHE_H];
static uint32_t dark_buf[CACHE_MAXW * CACHE_H];
Target CACHE_LIT = { lit_buf, CACHE_MAXW, CACHE_H };
Target CACHE_DARK = { dark_buf, CACHE_MAXW, CACHE_H };
uint8_t cache_row_alpha[CACHE_H];
int light_col[NFLOORS][MAXW * TILE];

/* ------------------------------------------------------------------ */
/* themes                                                              */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t wall_top, wall_bot, wains, trim, floor1, floor2, wood, wood_dark, door, door_dark;
    float amb[3], lamp[3], lamp2[3];
    int lamp_style;
} Theme;

static const Theme THEMES[TH_COUNT] = {
    /* house  */ { 0xFF9A8670, 0xFF85715A, 0xFF5E4634, 0xFFD8C8A8, 0xFF8A5A34, 0xFF74482A, 0xFF7A5030, 0xFF553620,
                   0xFF8A5A30, 0xFF5E3A1E, {0.52f,0.60f,0.95f}, {1.30f,1.04f,0.70f}, {1.30f,1.04f,0.70f}, 0 },
    /* store  */ { 0xFFA8B0A2, 0xFF959D90, 0xFFDCE0D8, 0xFF5A7A6A, 0xFFB8B6AE, 0xFFA2A098, 0xFF8A8E90, 0xFF5A5E62,
                   0xFF7A8A96, 0xFF4E5A64, {0.55f,0.65f,0.95f}, {1.05f,1.18f,1.05f}, {1.05f,1.18f,1.05f}, 1 },
    /* office */ { 0xFF8E9CAC, 0xFF7A8898, 0xFF6E7C8C, 0xFF3E4652, 0xFF4A586E, 0xFF404C62, 0xFF6A5444, 0xFF4A382C,
                   0xFF6E5A48, 0xFF4A3A2E, {0.52f,0.60f,0.95f}, {1.05f,1.12f,1.25f}, {1.05f,1.12f,1.25f}, 1 },
    /* club   */ { 0xFF4E3260, 0xFF3A2448, 0xFF2A1A34, 0xFF1A1020, 0xFF2E2E3C, 0xFF17171F, 0xFF4A3026, 0xFF2A1A12,
                   0xFF7A2236, 0xFF46121E, {0.60f,0.48f,0.95f}, {1.55f,0.50f,1.30f}, {0.45f,1.20f,1.55f}, 2 },
    /* tower  */ { 0xFF7E827C, 0xFF6A6E68, 0xFF55595A, 0xFF3A3E40, 0xFF4E5458, 0xFF3A3F44, 0xFF6A5A40, 0xFF4A3E2C,
                   0xFF5A6670, 0xFF3A444C, {0.50f,0.58f,0.85f}, {1.35f,1.05f,0.60f}, {1.35f,1.05f,0.60f}, 3 },
};
static const Theme *TH;

/* ------------------------------------------------------------------ */
/* rooms and lamps                                                     */
/* ------------------------------------------------------------------ */
#define MAXROOMS 24
typedef struct { int t0, t1, nl; float lx[4]; } Room;
static Room rooms[NFLOORS][MAXROOMS];
static int  nrooms[NFLOORS];
static int8_t room_of[NFLOORS][MAXW];
static int BW, CW;

static void find_rooms(void) {
    for (int f = 0; f < NFLOORS; f++) {
        nrooms[f] = 0;
        memset(room_of[f], -1, sizeof room_of[f]);
        if (!floor_active[f]) continue;
        int t = 0;
        while (t < W) {
            if (map[f][t] == '#' || map[f][t] == 'D') { t++; continue; }
            int s = t;
            while (t < W && map[f][t] != '#' && map[f][t] != 'D') t++;
            if (nrooms[f] >= MAXROOMS) continue;
            Room *r = &rooms[f][nrooms[f]];
            r->t0 = s; r->t1 = t - 1;
            float x0 = s * TILE, x1 = t * TILE, w = x1 - x0;
            r->nl = (int)(w / 104) + 1; if (r->nl > 4) r->nl = 4;
            for (int k = 0; k < r->nl; k++) r->lx[k] = x0 + w * (k + 0.5f) / r->nl;
            for (int k = s; k < t; k++) room_of[f][k] = (int8_t)nrooms[f];
            nrooms[f]++;
        }
        for (int k = 0; k < W; k++)   /* door tiles borrow a neighbour's room */
            if (map[f][k] == 'D') room_of[f][k] = k > 0 && room_of[f][k - 1] >= 0 ? room_of[f][k - 1] :
                                                  (k + 1 < W ? room_of[f][k + 1] : -1);
    }
}

static int is_shaft(int f, float x) { /* stairwell horizontal footprint on floor f */
    for (int t = 0; t < W; t++) {
        if (map[f][t] != 'S') continue;
        if (x >= t * TILE - 10 && x <= t * TILE + 26) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* wall / floor surfaces                                               */
/* ------------------------------------------------------------------ */
static uint32_t noise_c(uint32_t c, int x, int y, int amt) {
    int n = (int)(hash32((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u) & 255) - 128;
    return cmul(c, 256 + n * amt / 128);
}

static uint32_t wall_px(int x, int y, int top, int g, int theme) {
    const Theme *T = &THEMES[theme];
    uint32_t c = cmix(T->wall_top, T->wall_bot, (y - top) * 256 / (g - top));
    switch (theme) {
    case TH_HOUSE:
        if (y >= g - 24) {
            c = T->wains;
            if (x % 22 == 0 || (x % 22 == 3 && y > g - 21 && y < g - 6)) c = cmul(c, 200);
            if (y == g - 21 || y == g - 6) c = cmul(c, 220);
        } else if ((x / 5) & 1) c = cmul(c, 266);
        if (y >= g - 26 && y < g - 23) c = T->trim;
        if (y >= g - 4) c = cmul(T->trim, 150);
        break;
    case TH_STORE:
        if (y >= g - 32) {
            c = T->wains;
            if (x % 8 == 0 || (g - y) % 8 == 0) c = cmul(c, 200);
        }
        if (y >= g - 34 && y < g - 32) c = T->trim;
        if (y >= g - 3) c = cmul(T->trim, 160);
        break;
    case TH_OFFICE:
        if (x % 48 == 0) c = cmul(c, 220);
        if (y >= g - 5) c = T->trim;
        break;
    case TH_CLUB:
        if (y % 5 == 0) c = cmul(c, 225);
        if (y >= g - 26) {
            c = T->wains;
            if (((x + y) % 10 == 0) || (((x - y) % 10 + 10) % 10 == 0)) c = cmul(c, 160);
        }
        if (y >= g - 3) c = 0xFF0A0A0E;
        break;
    case TH_TOWER:
        if (x % 32 == 0 || (y - top) % 26 == 0) c = cmul(c, 205);
        if ((x % 32 == 4 || x % 32 == 28) && ((y - top) % 26 == 4 || (y - top) % 26 == 22)) c = cmul(c, 150);
        if (y >= top + 10 && y < top + 15) c = cmix(0xFF5A6068, 0xFF3A3E44, (y - top - 10) * 51);   /* pipe */
        if (y >= g - 6) c = (((x + y) / 4) & 1) ? 0xFFC89A30 : 0xFF1A1A1C;
        break;
    }
    return noise_c(c, x, y, 6);
}

static uint32_t floor_px(int x, int y, int g, int theme) {
    const Theme *T = &THEMES[theme];
    int d = y - g; /* 0..2 top face */
    uint32_t c;
    switch (theme) {
    case TH_HOUSE: {
        int row = d, plank = (x + row * 9) / 26;
        c = (plank & 1) ? T->floor1 : T->floor2;
        if ((x + row * 9) % 26 == 0) c = cmul(c, 170);
        c = cmul(c, 256 + (int)(hash32(plank) & 31) - 16);
    } break;
    case TH_STORE: c = (x % 16 == 0) ? cmul(T->floor1, 200) : T->floor1; break;
    case TH_CLUB: c = ((x / 8) & 1) ? T->floor1 : T->floor2; break;
    case TH_TOWER: c = (x % 3 == 0) ? cmul(T->floor1, 140) : T->floor1; break;
    default: c = T->floor1; break;
    }
    if (d == 0) c = cmul(c, 300);
    return noise_c(c, x, y, 10);
}

static void draw_brick(int x0, int y0, int w, int h, uint32_t base, uint32_t mortar) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            int row = y / 6, off = (row & 1) ? 7 : 0;
            int bx = (x + off) / 14;
            uint32_t c;
            if (y % 6 == 5 || (x + off) % 14 == 13) c = mortar;
            else c = cmul(base, 230 + (int)(hash32((uint32_t)(bx * 131 + row * 977)) & 63));
            px_set(x, y, noise_c(c, x, y, 8));
        }
}

/* ------------------------------------------------------------------ */
/* props                                                               */
/* ------------------------------------------------------------------ */
enum {
    P_COUCH, P_TV, P_SHELF, P_PLANT, P_FLOORLAMP, P_TABLE, P_DRESSER, P_DESK, P_FILE, P_COOLER,
    P_VENDING, P_FRIDGE, P_GONDOLA, P_COUNTER, P_CRATES, P_SERVER, P_BAR, P_SPEAKER, P_ARMCHAIR,
    W_WINDOW, W_PICTURE, W_POSTER, W_CLOCK, W_NEON, W_BOARD, W_VENT, P_NPROPS
};
static const int PROP_W[P_NPROPS] = {
    44, 30, 26, 14, 10, 40, 24, 44, 14, 10, 24, 32, 38, 44, 32, 20, 60, 20, 26,
    30, 20, 16, 10, 28, 36, 24 };
static const int PROP_H[P_NPROPS] = {
    18, 30, 44, 30, 48, 18, 26, 34, 30, 34, 46, 48, 32, 22, 28, 50, 52, 40, 20,
    0, 0, 0, 0, 0, 0, 0 };

static const int FLOOR_SET[TH_COUNT][8] = {
    { P_COUCH, P_TV, P_SHELF, P_PLANT, P_FLOORLAMP, P_TABLE, P_DRESSER, -1 },
    { P_GONDOLA, P_FRIDGE, P_GONDOLA, P_COUNTER, P_CRATES, P_VENDING, P_PLANT, -1 },
    { P_DESK, P_FILE, P_DESK, P_COOLER, P_PLANT, P_SHELF, P_VENDING, -1 },
    { P_BAR, P_SPEAKER, P_ARMCHAIR, P_TABLE, P_SPEAKER, P_PLANT, -1, -1 },
    { P_SERVER, P_CRATES, P_FILE, P_SERVER, P_DESK, P_CRATES, -1, -1 },
};
static const int WALL_SET[TH_COUNT][6] = {
    { W_WINDOW, W_PICTURE, W_CLOCK, W_PICTURE, W_WINDOW, -1 },
    { W_POSTER, W_WINDOW, W_CLOCK, W_POSTER, -1, -1 },
    { W_WINDOW, W_BOARD, W_PICTURE, W_CLOCK, W_WINDOW, -1 },
    { W_NEON, W_POSTER, W_NEON, W_PICTURE, -1, -1 },
    { W_VENT, W_POSTER, W_BOARD, W_VENT, -1, -1 },
};

typedef struct { int type, x, g, f; uint32_t seed; } PropInst;
#define MAXPROPS 160
static PropInst props[MAXPROPS];
static int nprops;

static const uint32_t FABRIC[] = { 0xFF2F6E6A, 0xFF7A2E3A, 0xFF5E6A3A, 0xFF46506E, 0xFF6E4A7A };
static const uint32_t BOOKS[] = { 0xFF9A3030, 0xFF2E5A8A, 0xFFC8A040, 0xFF3A7A4A, 0xFF7A4A8A, 0xFFD0D0C0, 0xFF8A5A2A };
static const uint32_t GOODS[] = { 0xFFE04848, 0xFFF0C040, 0xFF48A0E0, 0xFF60C060, 0xFFF08A30, 0xFFE070C0, 0xFFF0F0F0 };

static void shaded_box(int x, int y, int w, int h, uint32_t c) {
    fill(x, y, w, h, c);
    fill(x, y, w, 1, cmul(c, 320));
    fill(x, y, 1, h, cmul(c, 290));
    fill(x + w - 1, y, 1, h, cmul(c, 170));
    fill(x, y + h - 1, w, 1, cmul(c, 150));
}

static void draw_prop(const PropInst *p, int phase, int dimA) {
    int x = p->x, g = p->g;
    uint32_t s = p->seed;
    const Theme *T = TH;
    int A = dimA; /* emissive alpha scale (256 = full) */
    switch (p->type) {
    case P_COUCH: if (!phase) {
        uint32_t c = FABRIC[s % 5], cd = cmul(c, 190);
        fill(x + 3, g - 3, 2, 3, 0xFF111111); fill(x + 39, g - 3, 2, 3, 0xFF111111);
        shaded_box(x + 2, g - 20, 40, 10, cd);
        shaded_box(x + 4, g - 12, 36, 7, c);
        fill(x + 22, g - 12, 1, 7, cmul(c, 170));
        shaded_box(x, g - 15, 6, 11, cd); shaded_box(x + 38, g - 15, 6, 11, cd);
        disc(x + 10, g - 15, 3.5f, FABRIC[(s >> 3) % 5]);
    } break;
    case P_TV:
        if (!phase) {
            shaded_box(x, g - 11, 30, 11, T->wood);
            fill(x + 15, g - 10, 1, 9, T->wood_dark);
            fill(x + 13, g - 14, 4, 3, 0xFF15161A);
            shaded_box(x + 2, g - 32, 26, 18, 0xFF15161A);
            fill(x + 4, g - 30, 22, 14, 0xFF0A0C10);
        } else {
            vgrad(x + 4, g - 30, 22, 14, cmul(0xFF2A6AD0, A), cmul(0xFF52C0E8, A));
            fill(x + 7, g - 24, 8, 5, cmul(0xFFE8F0FF, A * 3 / 4));
            glow(x + 15, g - 23, 34, 0xFF3A70FF, 60 * A / 256);
        }
        break;
    case P_SHELF: if (!phase) {
        shaded_box(x, g - 44, 26, 44, T->wood_dark);
        fill(x + 2, g - 42, 22, 40, cmul(T->wood_dark, 160));
        for (int sh = 0; sh < 3; sh++) {
            int sy = g - 31 + sh * 13;
            fill(x + 1, sy, 24, 2, T->wood);
            int bx = x + 3, k = 0;
            while (bx < x + 22) {
                uint32_t h = hash32(s + sh * 31 + k++);
                int bw = 2 + (int)(h % 2), bh = 7 + (int)((h >> 4) % 4);
                if (bx + bw > x + 23) break;
                if ((h >> 9) % 7 == 0) { bx += 3; continue; }
                fill(bx, sy - bh, bw, bh, BOOKS[(h >> 12) % 7]);
                fill(bx, sy - bh, 1, bh, cmul(BOOKS[(h >> 12) % 7], 300));
                bx += bw;
            }
        }
    } break;
    case P_PLANT: if (!phase) {
        for (int i = 0; i < 10; i++) fill(x + 3 + i / 4, g - 10 + i, 8 - i / 2, 1, cmul(0xFFA0552E, 256 - i * 6));
        fill(x + 2, g - 11, 10, 2, 0xFF8A4A26);
        for (int k = 0; k < 7; k++) {
            uint32_t h = hash32(s + k * 17);
            float ex = x + 7 + ((int)(h % 15) - 7), ey = g - 18 - (int)((h >> 5) % 14);
            capsule(x + 7, g - 11, ex, ey, 1.0f, 0xFF2E5A2A);
            disc(ex, ey, 2.6f, (k & 1) ? 0xFF3E8A3A : 0xFF2E6E30);
            disc(ex - 0.6f, ey - 0.8f, 1.2f, 0xFF5AAA4A);
        }
    } break;
    case P_FLOORLAMP:
        if (!phase) {
            fill(x + 1, g - 2, 8, 2, 0xFF222428);
            fill(x + 4, g - 42, 2, 40, 0xFF5A5E66);
            for (int i = 0; i < 8; i++) fill(x + 1 + (7 - i) / 3, g - 50 + i, 8 - 2 * ((7 - i) / 3), 1, 0xFFD8C8A0);
        } else {
            for (int i = 0; i < 8; i++) fill(x + 1 + (7 - i) / 3, g - 50 + i, 8 - 2 * ((7 - i) / 3), 1, cmul(0xFFFFE8B8, A));
            glow(x + 5, g - 44, 46, 0xFFFFC070, 80 * A / 256);
        }
        break;
    case P_TABLE: if (!phase) {
        uint32_t ch = T->wood_dark;
        fill(x + 1, g - 22, 2, 22, ch); fill(x + 1, g - 10, 8, 2, ch); fill(x + 7, g - 8, 2, 8, ch);
        fill(x + 37, g - 22, 2, 22, ch); fill(x + 31, g - 10, 8, 2, ch); fill(x + 31, g - 8, 2, 8, ch);
        shaded_box(x + 9, g - 17, 22, 3, T->wood);
        fill(x + 11, g - 14, 2, 14, T->wood_dark); fill(x + 27, g - 14, 2, 14, T->wood_dark);
        if (s & 1) { fill(x + 18, g - 21, 3, 4, 0xFFD8D8D0); fill(x + 18, g - 21, 3, 1, 0xFF7A3020); }
    } break;
    case P_DRESSER: if (!phase) {
        shaded_box(x, g - 26, 24, 24, T->wood);
        fill(x + 1, g - 2, 3, 2, T->wood_dark); fill(x + 20, g - 2, 3, 2, T->wood_dark);
        for (int d = 0; d < 3; d++) { fill(x + 2, g - 18 + d * 7, 20, 1, T->wood_dark); fill(x + 11, g - 22 + d * 7, 3, 1, 0xFFC8A860); }
        shaded_box(x + 4, g - 34, 7, 8, 0xFF2A2A2E); fill(x + 5, g - 33, 5, 6, FABRIC[s % 5]);
    } break;
    case P_DESK:
        if (!phase) {
            shaded_box(x, g - 19, 34, 3, T->wood);
            fill(x + 2, g - 16, 2, 16, T->wood_dark);
            shaded_box(x + 22, g - 16, 11, 16, T->wood_dark);
            fill(x + 23, g - 10, 9, 1, cmul(T->wood_dark, 150));
            fill(x + 13, g - 23, 3, 4, 0xFF202226); fill(x + 10, g - 20, 9, 1, 0xFF202226);
            shaded_box(x + 6, g - 36, 18, 13, 0xFF1A1C22);
            fill(x + 8, g - 34, 14, 9, 0xFF080A0E);
            fill(x + 34, g - 13, 9, 3, 0xFF2A2E36); fill(x + 41, g - 27, 3, 15, 0xFF2A2E36);
            fill(x + 37, g - 10, 2, 7, 0xFF3A3E46); fill(x + 33, g - 3, 10, 2, 0xFF3A3E46);
        } else {
            vgrad(x + 8, g - 34, 14, 9, cmul(0xFF2E5878, A), cmul(0xFF1E3A56, A));
            for (int l = 0; l < 4; l++) fill(x + 9, g - 33 + l * 2, 4 + (int)(hash32(s + l) % 9), 1, cmul(0xFF9AD8FF, A));
            glow(x + 15, g - 30, 26, 0xFF5AA0E0, 50 * A / 256);
        }
        break;
    case P_FILE: if (!phase) {
        shaded_box(x, g - 30, 14, 30, 0xFF7A808A);
        for (int d = 0; d < 3; d++) { fill(x + 1, g - 21 + d * 10, 12, 1, 0xFF50565E); fill(x + 5, g - 26 + d * 10, 4, 2, 0xFFB0B6C0); }
    } break;
    case P_COOLER: if (!phase) {
        shaded_box(x, g - 22, 10, 22, 0xFFD0D4D8);
        fill(x + 2, g - 16, 6, 4, 0xFF3A3E44);
        disc(x + 5, g - 28, 4.5f, 0xFF6AA8D8); fill(x + 1, g - 30, 8, 8, 0xFF6AA8D8);
        fill(x + 2, g - 33, 2, 6, 0xFFA8D8F8);
    } break;
    case P_VENDING:
        if (!phase) {
            uint32_t body = (s & 1) ? 0xFFB02A2A : 0xFF2A5AA0;
            shaded_box(x, g - 46, 24, 46, body);
            fill(x + 2, g - 42, 15, 28, 0xFF101418);
            fill(x + 18, g - 40, 4, 8, 0xFF1A1C22);
            fill(x + 3, g - 10, 14, 5, 0xFF0A0A0C);
        } else {
            fill(x + 2, g - 42, 15, 28, cmul(0xFF8AA0B0, A * 2 / 3));
            for (int r = 0; r < 4; r++) for (int k = 0; k < 4; k++)
                fill(x + 3 + k * 4, g - 40 + r * 7, 2, 4, cmul(GOODS[hash32(s + r * 4 + k) % 7], A));
            fill(x + 18, g - 40, 4, 3, cmul(0xFF40FF80, A));
            glow(x + 10, g - 28, 34, 0xFFA0C8FF, 55 * A / 256);
        }
        break;
    case P_FRIDGE:
        if (!phase) {
            shaded_box(x, g - 48, 32, 48, 0xFF5A6068);
            fill(x + 2, g - 46, 13, 40, 0xFF12161C); fill(x + 17, g - 46, 13, 40, 0xFF12161C);
        } else {
            for (int dr = 0; dr < 2; dr++) {
                int dx0 = x + 2 + dr * 15;
                fill(dx0, g - 46, 13, 40, cmul(0xFFB8D8E8, A / 2));
                for (int sh = 0; sh < 4; sh++) {
                    fill(dx0, g - 36 + sh * 10, 13, 1, cmul(0xFFE0F0F8, A));
                    for (int k = 0; k < 4; k++)
                        fill(dx0 + 1 + k * 3, g - 42 + sh * 10, 2, 6, cmul(GOODS[hash32(s + dr * 40 + sh * 8 + k) % 7], A));
                }
                fill(dx0 + 11, g - 30, 1, 10, cmul(0xFFFFFFFF, A / 2));
            }
            glow(x + 16, g - 26, 44, 0xFFB0E0FF, 60 * A / 256);
        }
        break;
    case P_GONDOLA: if (!phase) {
        fill(x, g - 32, 2, 32, 0xFF6A7078); fill(x + 36, g - 32, 2, 32, 0xFF6A7078);
        fill(x + 2, g - 32, 34, 32, 0xFF3A3E44);
        for (int sh = 0; sh < 3; sh++) {
            int sy = g - 22 + sh * 10;
            fill(x, sy, 38, 2, 0xFF9AA0A8);
            int bx = x + 3, k = 0;
            while (bx < x + 34) {
                uint32_t h = hash32(s + sh * 53 + k++);
                int bw = 3 + (int)(h % 4), bh = 5 + (int)((h >> 5) % 4);
                if (bx + bw > x + 35) break;
                uint32_t c = GOODS[(h >> 9) % 7];
                fill(bx, sy - bh, bw, bh, c); fill(bx, sy - bh, bw, 1, cmul(c, 300));
                fill(bx + 1, sy - bh + 2, bw - 2 > 0 ? bw - 2 : 1, 1, 0xFFF8F8F8);
                bx += bw + 1;
            }
        }
    } break;
    case P_COUNTER:
        if (!phase) {
            shaded_box(x, g - 20, 44, 20, T->wood_dark);
            fill(x - 1, g - 22, 46, 3, 0xFFB8BCC0);
            shaded_box(x + 28, g - 30, 12, 8, 0xFF2A2E34);
            fill(x + 30, g - 29, 8, 3, 0xFF0A0C0E);
            fill(x + 4, g - 26, 8, 4, 0xFFE0D0A0);
        } else {
            fill(x + 30, g - 29, 8, 3, cmul(0xFF50F090, A));
            glow(x + 34, g - 28, 14, 0xFF50F090, 50 * A / 256);
        }
        break;
    case P_CRATES: if (!phase) {
        uint32_t c = 0xFF9A7A4A;
        for (int k = 0; k < 3; k++) {
            int bx = x + (k == 2 ? 8 : k * 16), by = k == 2 ? g - 28 : g - 14, w = 15;
            shaded_box(bx, by, w, 14, c);
            fill(bx + 1, by + 4, w - 2, 1, cmul(c, 180)); fill(bx + 1, by + 9, w - 2, 1, cmul(c, 180));
            capsule(bx + 2, by + 2, bx + w - 3, by + 11, 0.5f, cmul(c, 170));
        }
    } break;
    case P_SERVER:
        if (!phase) {
            shaded_box(x, g - 50, 20, 50, 0xFF16181C);
            for (int u = 0; u < 9; u++) { fill(x + 2, g - 47 + u * 5, 16, 4, 0xFF22252B); fill(x + 2, g - 44 + u * 5, 16, 1, 0xFF0C0D10); }
        } else {
            for (int u = 0; u < 9; u++) for (int k = 0; k < 3; k++) {
                uint32_t h = hash32(s + u * 7 + k);
                if (h % 3 == 0) continue;
                fill(x + 4 + k * 3, g - 46 + u * 5, 1, 1, cmul((h & 8) ? 0xFF40FF70 : 0xFFFFB030, A));
            }
            glow(x + 10, g - 26, 26, 0xFF40FF70, 22 * A / 256);
        }
        break;
    case P_BAR:
        if (!phase) {
            fill(x + 2, g - 54, 56, 2, T->wood_dark); fill(x + 2, g - 42, 56, 2, T->wood_dark);
            shaded_box(x, g - 21, 60, 21, T->wood);
            fill(x, g - 7, 60, 1, 0xFFC8A040);
            fill(x - 2, g - 23, 64, 3, 0xFF1A1012);
            for (int k = 0; k < 2; k++) { int sx = x + 10 + k * 36; disc(sx, g - 14, 4, 0xFF2A2A30); fill(sx - 1, g - 12, 2, 12, 0xFF5A5E66); }
        } else {
            for (int r = 0; r < 2; r++) for (int k = 0; k < 9; k++) {
                uint32_t h = hash32(s + r * 20 + k);
                int bx = x + 5 + k * 6, by = g - 54 + 12 * r;
                uint32_t c = cmul(GOODS[h % 7], A * 3 / 4);
                fill(bx, by - 8, 3, 8, c); fill(bx + 1, by - 11, 1, 3, c);
            }
            fill(x + 2, g - 40, 56, 1, cmul(0xFFFF60D0, A));
            glow(x + 30, g - 44, 48, 0xFFFF50C8, 45 * A / 256);
        }
        break;
    case P_SPEAKER: if (!phase) {
        shaded_box(x, g - 40, 20, 40, 0xFF121214);
        disc(x + 10, g - 30, 5.5f, 0xFF2A2A30); disc(x + 10, g - 30, 2, 0xFF0A0A0C);
        disc(x + 10, g - 13, 7.5f, 0xFF2A2A30); disc(x + 10, g - 13, 3, 0xFF0A0A0C);
        ring(x + 10, g - 13, 7.5f, 0xFF3A3A42);
    } break;
    case P_ARMCHAIR: if (!phase) {
        uint32_t c = 0xFF5A1A2A;
        shaded_box(x + 2, g - 22, 22, 12, cmul(c, 200));
        shaded_box(x + 3, g - 12, 20, 8, c);
        shaded_box(x, g - 16, 5, 12, cmul(c, 180)); shaded_box(x + 21, g - 16, 5, 12, cmul(c, 180));
        fill(x + 3, g - 4, 2, 4, 0xFF111111); fill(x + 21, g - 4, 2, 4, 0xFF111111);
    } break;
    /* ---- wall props ---- */
    case W_WINDOW:
        if (!phase) {
            if (p->f >= 0 && (cur_level == TH_HOUSE || TH == &THEMES[TH_HOUSE] || TH == &THEMES[TH_OFFICE])) {}
            shaded_box(x, g - 68, 30, 40, 0xFFB8B0A0);
            fill(x + 2, g - 66, 26, 36, 0xFF0E1420);
            fill(x - 2, g - 29, 34, 3, 0xFFC8C0B0);
            if (TH == &THEMES[TH_HOUSE]) {
                uint32_t cc = FABRIC[s % 5];
                for (int y = g - 70; y < g - 26; y++) {
                    int sw = 6 - (y - (g - 70)) / 12;
                    for (int k = 0; k < sw; k++) {
                        uint32_t c = ((k + y / 3) % 3 == 0) ? cmul(cc, 190) : cc;
                        px_set(x - 3 + k, y, c); px_set(x + 32 - k, y, c);
                    }
                }
                fill(x - 5, g - 72, 40, 2, 0xFF3A3A3E);
            } else if (TH == &THEMES[TH_OFFICE]) {
                for (int y = g - 66; y < g - 46; y += 3) fill(x + 2, y, 26, 1, 0xFFB8BCC0);
            }
        } else {
            vgrad(x + 2, g - 66, 26, 36, cmul(0xFF101A30, A), cmul(0xFF26365A, A));
            if (TH == &THEMES[TH_OFFICE]) for (int y = g - 66; y < g - 46; y += 3) fill(x + 2, y, 26, 1, cmul(0xFF9AA4B0, A));
            for (int k = 0; k < 14; k++) {
                uint32_t h = hash32(s + k * 3);
                int wx = x + 3 + (int)(h % 24), wy = g - 46 + (int)((h >> 8) % 14);
                px_set(wx, wy, cmul((h & 16) ? 0xFFFFD890 : 0xFFA8C0FF, A));
            }
            fill(x + 14, g - 66, 2, 36, 0xFFB8B0A0);
            fill(x + 2, g - 49, 26, 2, 0xFFB8B0A0);
            line_a(x + 4, g - 36, x + 12, g - 62, 0xFFFFFFFF, 40 * A / 256);
        }
        break;
    case W_PICTURE: if (!phase) {
        uint32_t fr = (s & 1) ? 0xFFC8A040 : 0xFF2A2A2E;
        shaded_box(x, g - 64, 20, 16, fr);
        uint32_t bg = GOODS[(s >> 2) % 7];
        fill(x + 2, g - 62, 16, 12, cmix(bg, 0xFF404040, 120));
        disc(x + 6 + (s >> 5) % 8, g - 57, 3, GOODS[(s >> 8) % 7]);
        fill(x + 2, g - 54, 16, 4, cmix(GOODS[(s >> 11) % 7], 0xFF202020, 100));
    } break;
    case W_POSTER: if (!phase) {
        uint32_t c = GOODS[s % 7];
        fill(x, g - 66, 16, 22, cmix(c, 0xFF101010, 60));
        fill(x + 2, g - 63, 12, 8, cmix(GOODS[(s >> 3) % 7], 0xFF000000, 40));
        for (int k = 0; k < 3; k++) fill(x + 2, g - 52 + k * 3, 12 - k * 3, 1, 0xFFF0F0E8);
        fill(x, g - 66, 2, 2, 0xFFD0D0D0); fill(x + 14, g - 66, 2, 2, 0xFFD0D0D0);
    } break;
    case W_CLOCK: if (!phase) {
        disc(x + 5, g - 62, 5.5f, 0xFF1A1A1E); disc(x + 5, g - 62, 4.5f, 0xFFF0EEE6);
        line_a(x + 5, g - 62, x + 5, g - 65.5f, 0xFF111111, 256);
        line_a(x + 5, g - 62, x + 7.5f, g - 61, 0xFF111111, 256);
    } break;
    case W_NEON:
        if (!phase) fill(x, g - 70, 28, 22, 0xFF120A16);
        else {
            uint32_t c = (s & 1) ? 0xFFFF4FD8 : 0xFF4FE3FF;
            float cx = x + 14, cy = g - 59;
            if (s & 2) { for (int k = 0; k < 4; k++) capsule(x + 3 + k * 6, cy + ((k & 1) ? 5 : -5), x + 9 + k * 6, cy + ((k & 1) ? -5 : 5), 1, cmul(c, A)); }
            else { ring(cx, cy, 7, cmul(c, A)); ring(cx, cy, 3, cmul(c, A)); }
            glow(cx, cy, 30, c, 90 * A / 256);
        }
        break;
    case W_BOARD: if (!phase) {
        shaded_box(x, g - 66, 36, 24, 0xFF8A8E96);
        fill(x + 2, g - 64, 32, 20, 0xFFF0F2F4);
        for (int k = 0; k < 4; k++) {
            uint32_t h = hash32(s + k);
            line_a(x + 4, g - 60 + k * 4, x + 8 + (int)(h % 20), g - 60 + k * 4 + (int)(h >> 8) % 3 - 1, (k & 1) ? 0xFF2A5AC8 : 0xFFC83A3A, 230);
        }
        fill(x + 4, g - 42, 28, 2, 0xFF6A6E76);
    } break;
    case W_VENT: if (!phase) {
        shaded_box(x, g - 70, 24, 16, 0xFF5A5E66);
        for (int k = 0; k < 6; k++) fill(x + 2, g - 68 + k * 2 + 1, 20, 1, 0xFF22252A);
    } break;
    }
}

static void place_props(int f) {
    int g = floor_ground(f);
    int th = LEVELS[cur_level].theme;
    for (int r = 0; r < nrooms[f]; r++) {
        Room *R = &rooms[f][r];
        int x0 = R->t0 * TILE + 8, x1 = (R->t1 + 1) * TILE - 8;
        uint32_t seed = hash32((uint32_t)(cur_level * 1000 + f * 100 + r));
        int start = nprops;
        /* floor layer */
        int x = x0 + (int)(seed % 10);
        int k = 0, last = -1;
        while (x < x1 && nprops < MAXPROPS) {
            uint32_t h = hash32(seed + k++ * 7919);
            int cnt = 0; while (cnt < 8 && FLOOR_SET[th][cnt] >= 0) cnt++;
            int type = FLOOR_SET[th][h % cnt];
            if (type == last || (type == P_FLOORLAMP && last >= 0 && (h & 1))) type = FLOOR_SET[th][(h / 7 + 1) % cnt];
            if (type == last) { x += 6; continue; }
            int w = PROP_W[type];
            int ok = x + w <= x1;
            for (int xx = x; ok && xx <= x + w; xx += 4) if (is_shaft(f, xx)) ok = 0;
            if (ok && (h >> 10) % 5 != 0) {
                props[nprops++] = (PropInst){ type, x, g, f, h };
                last = type;
                x += w + 8 + (int)((h >> 14) % 22);
            } else x += 10;
        }
        int end = nprops;
        /* wall layer: avoid tall furniture and stairwells */
        x = x0 + 4 + (int)((seed >> 8) % 12);
        int used_clock = 0; last = -1;
        while (x < x1 && nprops < MAXPROPS) {
            uint32_t h = hash32(seed + k++ * 104729);
            int cnt = 0; while (cnt < 6 && WALL_SET[th][cnt] >= 0) cnt++;
            int type = WALL_SET[th][h % cnt];
            if (type == last || (type == W_CLOCK && used_clock)) type = WALL_SET[th][(h / 5 + 1) % cnt];
            if (type == last || (type == W_CLOCK && used_clock)) { x += 8; continue; }
            int w = PROP_W[type];
            int ok = x + w <= x1;
            for (int xx = x - 4; ok && xx <= x + w + 4; xx += 4) if (is_shaft(f, xx)) ok = 0;
            for (int i = start; ok && i < end; i++)
                if (PROP_H[props[i].type] > 30 && x < props[i].x + PROP_W[props[i].type] + 3 && props[i].x < x + w + 3) ok = 0;
            if (ok && (h >> 12) % 4 != 0) {
                props[nprops++] = (PropInst){ type, x, g, f, h };
                last = type; if (type == W_CLOCK) used_clock = 1;
                x += w + 18 + (int)((h >> 16) % 30);
            } else x += 12;
        }
    }
}

/* ------------------------------------------------------------------ */
/* building pieces                                                     */
/* ------------------------------------------------------------------ */
static int first_open[NFLOORS], last_open[NFLOORS];

static void draw_interior(int f) {
    int th = LEVELS[cur_level].theme;
    int top = floor_top(f), g = floor_ground(f);
    for (int t = 0; t < W; t++) {
        int x0 = t * TILE;
        if (map[f][t] == '#') {
            if (t < first_open[f] || t > last_open[f]) draw_brick(x0, top, TILE, g - top, 0xFF7A4434, 0xFF3A2A26);
            else {
                for (int x = x0; x < x0 + TILE; x++)
                    for (int y = top; y < g; y++) {
                        uint32_t c = 0xFF4E525A;
                        if ((y - top) % 22 == 0) c = 0xFF3E4148;
                        if (x == x0 || (t > 0 && map[f][t - 1] != '#' && x < x0 + 2)) c = 0xFF6A6E76;
                        if (x == x0 + TILE - 1) c = 0xFF34373D;
                        px_set(x, y, noise_c(c, x, y, 10));
                    }
            }
            continue;
        }
        for (int x = x0; x < x0 + TILE; x++) {
            for (int y = top; y < top + 8; y++) px_set(x, y, noise_c(cmix(0xFF1C1E24, 0xFF30333A, (y - top) * 32), x, y, 6));
            px_set(x, top + 8, cmul(THEMES[th].trim, th == TH_CLUB ? 256 : 200));
            for (int y = top + 9; y < g; y++) px_set(x, y, wall_px(x, y, top, g, th));
        }
    }
    /* floor surface + slab */
    for (int x = 0; x < BW; x++) {
        int t = x / TILE;
        for (int y = g; y < g + 3; y++) px_set(x, y, map[f][t] == '#' ? 0xFF3A3D44 : floor_px(x, y, g, th));
        for (int y = g + 3; y < g + SLAB; y++) px_set(x, y, noise_c(y == g + SLAB - 1 ? 0xFF202228 : 0xFF34373E, x, y, 14));
    }
    /* door frames */
    for (int t = 0; t < W; t++) {
        if (map[f][t] != 'D') continue;
        int x = t * TILE;
        for (int yy = g - DOOR_H; yy < g; yy++)
            for (int xx = x + 3; xx < x + 13; xx++) {
                uint32_t *p = &TG->px[(yy + TOY) * TG->w + xx + TOX];
                *p = cmul(*p, 170);
            }
        uint32_t tr = th == TH_CLUB ? 0xFF2A2030 : th == TH_TOWER ? 0xFF4A5058 : 0xFFC8BCA4;
        shaded_box(x + 1, g - DOOR_H - 3, 14, 3, tr);
        fill(x + 1, g - DOOR_H, 2, DOOR_H, tr); fill(x + 13, g - DOOR_H, 2, DOOR_H, tr);
        fill(x + 2, g - DOOR_H, 1, DOOR_H, cmul(tr, 300));
        fill(x + 13, g - DOOR_H, 1, DOOR_H, cmul(tr, 160));
        fill(x + 3, g - 1, 10, 1, 0xFF15161A);
    }
    /* ceiling lamp fixtures */
    for (int r = 0; r < nrooms[f]; r++)
        for (int k = 0; k < rooms[f][r].nl; k++) {
            float lx = rooms[f][r].lx[k];
            switch (THEMES[th].lamp_style) {
            case 0: fill((int)lx, top + 8, 1, 8, 0xFF202226);
                    for (int i = 0; i < 6; i++) fill((int)lx - 2 - i, top + 16 + i, 5 + 2 * i, 1, cmul(0xFFC8B890, 256 - i * 12)); break;
            case 1: fill((int)lx - 14, top + 8, 28, 4, 0xFF8A9098); fill((int)lx - 13, top + 11, 26, 2, 0xFFE8F0F0); break;
            case 2: fill((int)lx - 1, top + 8, 2, 4, 0xFF202226); disc(lx, top + 14, 3.5f, 0xFF2A2A30); break;
            case 3: fill((int)lx, top + 8, 1, 5, 0xFF202226); disc(lx, top + 16, 3.5f, 0xFF3A3020); ring(lx, top + 16, 3.5f, 0xFF15161A); break;
            }
        }
    place_props(f);
}

static void draw_facade(int f) {
    int top = floor_top(f);
    draw_brick(0, top, BW, FLOOR_H - SLAB, 0xFF6A3E32, 0xFF34241F);
    for (int x = 0; x < BW; x++)
        for (int y = top + FLOOR_H - SLAB; y < top + FLOOR_H; y++)
            px_set(x, y, noise_c(y == top + FLOOR_H - SLAB ? 0xFF8A8C90 : 0xFF5C5E64, x, y, 10));
    for (int x = 24; x + 26 < BW; x += 58) {
        int wy = top + 34;
        shaded_box(x - 2, wy - 2, 30, 46, 0xFF9A948A);
        fill(x, wy, 26, 42, 0xFF141A26);
        fill(x - 4, wy + 44, 34, 3, 0xFFB0AAA0);
    }
}

static void draw_facade_emissive(int f) {
    int top = floor_top(f);
    for (int x = 24; x + 26 < BW; x += 58) {
        int wy = top + 34;
        uint32_t h = hash32((uint32_t)(x * 31 + f * 7 + cur_level * 1013));
        if (h % 3 == 0) {
            vgrad(x, wy, 26, 42, 0xFFE8B070, 0xFFB07840);
            for (int y = wy; y < wy + 42; y += 3) fill(x, y, 26, 1, 0xFF8A5A30);
            if (h & 8) fill(x + 6 + (int)(h >> 8) % 10, wy + 18, 6, 24, 0xFF2A1A12);
            glow(x + 13, wy + 21, 44, 0xFFE8A050, 40);
        } else {
            vgrad(x, wy, 26, 42, 0xFF141C2C, 0xFF243048);
            line_a(x + 3, wy + 38, x + 15, wy + 4, 0xFFFFFFFF, 30);
        }
        fill(x + 12, wy, 2, 42, 0xFF9A948A);
        fill(x, wy + 20, 26, 2, 0xFF9A948A);
    }
}

static void draw_stairs(void) {
    for (int f = 0; f < NFLOORS - 1; f++)
        for (int t = 0; t < W; t++) {
            if (map[f][t] != 'S' || map[f + 1][t] != 'S') continue;
            int gU = floor_ground(f), gL = floor_ground(f + 1);
            int sx = t * TILE - 6, sw = 28;
            for (int y = gU; y < gL; y++)
                for (int x = sx; x < sx + sw; x++) {
                    uint32_t c = cmix(0xFF2C2F36, 0xFF24272D, (y - gU) * 256 / (gL - gU));
                    if (x == sx || x == sx + sw - 1) c = 0xFF4A4E56;
                    px_set(x, y, noise_c(c, x, y, 8));
                }
            int mid = (gU + gL) / 2;
            for (int fl = 0; fl < 2; fl++) {
                float ax = fl ? sx + sw - 3 : sx + 3, ay = fl ? mid : gL;
                float bx = fl ? sx + 3 : sx + sw - 3, by = fl ? gU : mid;
                int n = 8;
                for (int k = 0; k < n; k++) {
                    float u = (float)k / n;
                    int stx = (int)(ax + (bx - ax) * u), sty = (int)(ay + (by - ay) * u);
                    int run = (int)fabsf((bx - ax) / n) + 1;
                    fill(fl ? stx - run : stx, sty - 7, run + 1, 7, 0xFF5A5E66);
                    fill(fl ? stx - run : stx, sty - 7, run + 1, 1, 0xFF8A8E96);
                }
                capsule(ax, ay, bx, by, 1.6f, 0xFF3A3E46);
                line_a(ax, ay - 14, bx, by - 14, 0xFFA0A4AC, 220);
            }
            fill(sx + 1, mid - 1, sw - 2, 3, 0xFF6A6E76);
            /* railing at the top landing */
            fill(sx, gU - 15, sw, 2, 0xFF9A9EA6);
            for (int x = sx; x < sx + sw; x += 6) fill(x, gU - 15, 1, 15, 0xFF7A7E86);
        }
}

static void draw_stair_emissive(int dimA) {
    for (int f = 0; f < NFLOORS; f++)
        for (int t = 0; t < W; t++) {
            if (map[f][t] != 'S' || !floor_active[f]) continue;
            int x = t * TILE + 1, y = floor_top(f) + 16;
            fill(x, y, 14, 8, cmul(0xFF1E9A52, dimA));
            uint32_t w = cmul(0xFFE8FFF0, dimA);
            fill(x + 3, y + 3, 6, 2, w); fill(x + 8, y + 2, 1, 4, w); fill(x + 9, y + 3, 1, 2, w);
            glow(x + 7, y + 4, 18, 0xFF30FF80, 50 * dimA / 256);
        }
}

static void draw_roof(void) {
    int y = ROOF - 10;
    for (int x = 0; x < BW; x++)
        for (int yy = y; yy < ROOF; yy++)
            px_set(x, yy, noise_c(yy == y ? 0xFF9A9EA4 : yy < y + 3 ? 0xFF6E7278 : 0xFF4E5258, x, yy, 10));
    int ax = BW / 4;
    shaded_box(ax, y - 16, 34, 16, 0xFF6A7078);
    for (int k = 0; k < 6; k++) fill(ax + 4 + k * 4, y - 13, 2, 10, 0xFF3A3E44);
    int tx = BW * 3 / 4;
    fill(tx + 3, y - 14, 2, 14, 0xFF2A2A2E); fill(tx + 23, y - 14, 2, 14, 0xFF2A2A2E);
    for (int yy = y - 40; yy < y - 14; yy++) {
        int inset = yy < y - 36 ? (y - 36 - yy) : 0;
        fill(tx + inset, yy, 28 - 2 * inset, 1, cmix(0xFF6A4A32, 0xFF4A3222, (yy - (y - 40)) * 10));
    }
    for (int yy = y - 36; yy < y - 14; yy += 5) fill(tx, yy, 28, 1, 0xFF2E2016);
    int px = BW / 2 + 20;
    fill(px, y - 50, 2, 50, 0xFF3A3E44); fill(px - 6, y - 42, 14, 1, 0xFF3A3E44); fill(px - 4, y - 34, 10, 1, 0xFF3A3E44);
}

static void draw_surroundings(void) {
    int sy = ROOF + NFLOORS * FLOOR_H;
    /* street */
    for (int x = -MARGIN; x < BW + MARGIN; x++) {
        for (int y = sy; y < sy + 10; y++) px_set(x, y, noise_c((x % 22 == 0) ? 0xFF3E4248 : y == sy ? 0xFF6A6E76 : 0xFF4E525A, x, y, 10));
        for (int y = sy + 10; y < sy + 13; y++) px_set(x, y, 0xFF72767E);
        for (int y = sy + 13; y < CACHE_H; y++) {
            uint32_t c = 0xFF1B1D23;
            if (y >= sy + 37 && y < sy + 39 && ((x + 4000) % 44) < 22) c = 0xFF8A8264;
            px_set(x, y, noise_c(c, x, y, 16));
        }
    }
    /* neighbours */
    int lt = ROOF + 24;
    draw_brick(-MARGIN, lt, MARGIN - 16, sy - lt, 0xFF4A3A44, 0xFF2A2026);
    fill(-MARGIN, lt - 4, MARGIN - 16, 4, 0xFF6A6870);
    for (int x = -MARGIN + 14; x < -40; x += 40)
        for (int y = lt + 16; y < sy - 30; y += 46) { fill(x - 1, y - 1, 22, 32, 0xFF7A747A); fill(x, y, 20, 30, 0xFF121824); }
    vgrad(-16, lt + 10, 16, sy - lt - 10, 0xFF0C0E12, 0xFF16181E);
    for (int y = lt + 30; y < sy - 10; y += 34) {      /* fire escape */
        fill(-40, y, 26, 2, 0xFF2A2C30);
        for (int x = -40; x < -14; x += 4) fill(x, y - 8, 1, 8, 0xFF2A2C30);
        line_a(-38, y, -16, y + 34, 0xFF2A2C30, 256);
    }
    int rt = ROOF - 30;
    for (int x = BW + 16; x < BW + MARGIN; x++)
        for (int y = rt; y < sy; y++) {
            uint32_t c = ((x - BW) % 30 == 0) ? 0xFF3A3E48 : 0xFF4C5260;
            px_set(x, y, noise_c(c, x, y, 10));
        }
    for (int x = BW + 26; x < BW + MARGIN - 16; x += 30)
        for (int y = rt + 14; y < sy - 24; y += 26) fill(x, y, 16, 18, 0xFF151A26);
    fill(BW + 16, rt - 4, MARGIN - 16, 4, 0xFF70747C);
    vgrad(BW, rt + 20, 16, sy - rt - 20, 0xFF0C0E12, 0xFF16181E);
    /* street lamps */
    int lamps[2] = { -70, BW + 80 };
    for (int k = 0; k < 2; k++) {
        int lx = lamps[k];
        fill(lx, sy - 70, 3, 70, 0xFF2A2E34);
        fill(lx - 10, sy - 72, 16, 3, 0xFF2A2E34);
        fill(lx - 13, sy - 70, 8, 3, 0xFF3A3E44);
    }
}

static void draw_surroundings_emissive(void) {
    int sy = ROOF + NFLOORS * FLOOR_H;
    int lt = ROOF + 24, rt = ROOF - 30;
    for (int x = -MARGIN + 14; x < -40; x += 40)
        for (int y = lt + 16; y < sy - 30; y += 46) {
            uint32_t h = hash32((uint32_t)(x * 7 + y * 13 + cur_level));
            if (h % 3 == 0) { vgrad(x, y, 20, 30, 0xFFD8A060, 0xFFA87040); fill(x + 9, y, 2, 30, 0xFF7A747A); }
        }
    for (int x = BW + 26; x < BW + MARGIN - 16; x += 30)
        for (int y = rt + 14; y < sy - 24; y += 26) {
            uint32_t h = hash32((uint32_t)(x * 11 + y * 5 + cur_level * 3));
            if (h % 4 == 0) vgrad(x, y, 16, 18, 0xFFB8D0F0, 0xFF7890B8);
        }
    int lamps[2] = { -70, BW + 80 };
    for (int k = 0; k < 2; k++) {
        int lx = lamps[k];
        fill(lx - 12, sy - 67, 6, 1, 0xFFFFF0C0);
        glow(lx - 9, sy - 64, 30, 0xFFFFD890, 90);
        for (int y = sy - 64; y < sy + 12; y++) {
            int hw = (y - (sy - 64)) * 2 / 3;
            fill_add(lx - 9 - hw, y, hw * 2 + 1, 1, 0xFFFFD890, 18);
        }
    }
    fill(BW + MARGIN / 2, rt - 12, 2, 8, 0xFF2A2A2E);
    disc(BW + MARGIN / 2 + 1, rt - 13, 1.6f, 0xFFFF3030);
    glow(BW + MARGIN / 2 + 1, rt - 13, 10, 0xFFFF2020, 120);
}

/* ------------------------------------------------------------------ */
/* lighting                                                            */
/* ------------------------------------------------------------------ */
static void light_floor(int f) {
    int top = floor_top(f), g = floor_ground(f);
    const Theme *T = TH;
    for (int x = 0; x < BW; x++) {
        int t = x / TILE;
        if (map[f][t] == '#') continue;
        int r = room_of[f][t];
        if (r < 0) continue;
        Room *R = &rooms[f][r];
        float rx0 = R->t0 * TILE, rx1 = (R->t1 + 1) * TILE;
        float edge = fminf(x - rx0, rx1 - x);
        float aoh = edge < 14 ? 0.78f + 0.22f * fmaxf(edge, 0) / 14 : 1.0f;
        for (int y = top; y < g + 3; y++) {
            float k[3] = { T->amb[0] * 0.36f, T->amb[1] * 0.36f, T->amb[2] * 0.36f };
            for (int l = 0; l < R->nl; l++) {
                const float *lc = (T->lamp_style == 2 && ((r + l) & 1)) ? T->lamp2 : T->lamp;
                float dx = x - R->lx[l], dy = y - (top + 16);
                float c = 0;
                if (dy > 0) {
                    float w = 10 + dy * 0.95f, u = fabsf(dx) / w;
                    if (u < 1) c = (1 - u) * (1 - u) * (0.95f - 0.35f * dy / FLOOR_H);
                }
                c += 0.75f * 50 / (50 + dx * dx + dy * dy);
                if (y >= g) c += 0.25f * fmaxf(0, 1 - fabsf(dx) / 60);
                for (int ch = 0; ch < 3; ch++) k[ch] += lc[ch] * c * 0.8f;
            }
            float v = (float)(y - top) / (g - top);
            float ao = aoh * (v < 0.14f ? 0.72f + 2.0f * v : 1.0f) * (y > g - 5 && y < g ? 0.86f : 1.0f);
            uint32_t *p = &TG->px[(y + TOY) * TG->w + x + TOX];
            uint32_t c = *p;
            int rr = (int)(((c >> 16) & 255) * k[0] * ao), gg = (int)(((c >> 8) & 255) * k[1] * ao), bb = (int)((c & 255) * k[2] * ao);
            if (rr > 255) rr = 255; if (gg > 255) gg = 255; if (bb > 255) bb = 255;
            *p = 0xFF000000u | (uint32_t)rr << 16 | (uint32_t)gg << 8 | (uint32_t)bb;
            if (y == g - 20) light_col[f][x] = (int)((k[0] + k[1] + k[2]) / 3 * ao * 256);
        }
    }
}

static void lamp_emissive(int f) {
    int top = floor_top(f);
    for (int r = 0; r < nrooms[f]; r++)
        for (int k = 0; k < rooms[f][r].nl; k++) {
            float lx = rooms[f][r].lx[k];
            const float *lc = (TH->lamp_style == 2 && ((r + k) & 1)) ? TH->lamp2 : TH->lamp;
            uint32_t col = 0xFF000000u | (uint32_t)fminf(255, lc[0] * 200) << 16 | (uint32_t)fminf(255, lc[1] * 200) << 8 | (uint32_t)fminf(255, lc[2] * 200);
            switch (TH->lamp_style) {
            case 0: fill((int)lx - 3, top + 21, 7, 2, 0xFFFFF4D0); break;
            case 1: fill((int)lx - 13, top + 11, 26, 2, 0xFFFFFFFF); break;
            case 2: disc(lx, top + 16, 2, 0xFFFFFFFF); break;
            case 3: disc(lx, top + 17, 2, 0xFFFFE8B0); break;
            }
            glow(lx, top + 18, 26, col, 120);
        }
}

/* ------------------------------------------------------------------ */
/* bake                                                                */
/* ------------------------------------------------------------------ */
void art_bake(void) {
    TH = &THEMES[LEVELS[cur_level].theme];
    BW = W * TILE; CW = BW + 2 * MARGIN;
    CACHE_LIT.w = CACHE_DARK.w = CW;
    memset(lit_buf, 0, sizeof(uint32_t) * CW * CACHE_H);
    memset(light_col, 0, sizeof light_col);
    for (int f = 0; f < NFLOORS; f++) {
        first_open[f] = W; last_open[f] = -1;
        for (int t = 0; t < W; t++) if (map[f][t] != '#') { if (t < first_open[f]) first_open[f] = t; last_open[f] = t; }
    }
    find_rooms();
    nprops = 0;

    tg_set(&CACHE_LIT, MARGIN, 0);
    draw_surroundings();
    draw_roof();
    for (int f = 0; f < NFLOORS; f++) {
        if (floor_active[f]) draw_interior(f); else draw_facade(f);
    }
    for (int i = 0; i < nprops; i++) draw_prop(&props[i], 0, 256);
    draw_stairs();
    for (int f = 0; f < NFLOORS; f++) if (floor_active[f]) light_floor(f);

    draw_surroundings_emissive();
    for (int f = 0; f < NFLOORS; f++) if (!floor_active[f]) draw_facade_emissive(f);

    memcpy(dark_buf, lit_buf, sizeof(uint32_t) * CW * CACHE_H);
    for (int f = 0; f < NFLOORS; f++) {
        if (!floor_active[f]) continue;
        for (int y = floor_top(f); y < floor_ground(f) + 3; y++)
            for (int x = 0; x < BW; x++) {
                if (map[f][x / TILE] == '#') continue;
                uint32_t *p = &dark_buf[y * CW + x + MARGIN];
                *p = cmix(cmul(*p, 52), 0xFF060A14, 60);
            }
    }
    /* interior emissives: full on lit, dimmed on dark */
    for (int pass = 0; pass < 2; pass++) {
        tg_set(pass ? &CACHE_DARK : &CACHE_LIT, MARGIN, 0);
        int A = pass ? 130 : 256;
        for (int f = 0; f < NFLOORS; f++) if (floor_active[f] && !pass) lamp_emissive(f);
        for (int i = 0; i < nprops; i++) draw_prop(&props[i], 1, A);
        draw_stair_emissive(A);
    }
    for (int y = 0; y < CACHE_H; y++) {
        cache_row_alpha[y] = 0;
        for (int x = 0; x < CW; x++) if (!(lit_buf[y * CW + x] >> 24)) { cache_row_alpha[y] = 1; break; }
    }
}

void art_add_bullet_hole(int f, float wx, int wy) {
    (void)f;
    for (int pass = 0; pass < 2; pass++) {
        tg_set(pass ? &CACHE_DARK : &CACHE_LIT, MARGIN, 0);
        int x = (int)wx;
        uint32_t c = TG->px[wy * TG->w + x + MARGIN];
        if (!(c >> 24)) continue;
        px_set(x, wy, cmul(c, 60)); px_set(x + 1, wy, cmul(c, 120));
        px_set(x, wy + 1, cmul(c, 120)); px_set(x - 1, wy - 1, cmul(c, 300));
    }
}

/* ------------------------------------------------------------------ */
/* sky (drawn live, behind the cache)                                  */
/* ------------------------------------------------------------------ */
void art_sky(Target *t, float camx, float camy, float time) {
    int y0 = camy == 40 ? 0 : HUD_H;   /* menus pass camy 40 and fill the whole screen */
    tg_set(t, 0, 0);
    tg_clip(0, y0, SCREEN_W, SCREEN_H);
    for (int sy = y0; sy < SCREEN_H; sy++) {
        float wy = sy - HUD_H + camy;
        float u = (wy + 260) / 560; if (u < 0) u = 0; if (u > 1) u = 1;
        uint32_t c = u < 0.6f ? cmix(0xFF04060E, 0xFF161A34, (int)(u / 0.6f * 256)) : cmix(0xFF161A34, 0xFF3A2638, (int)((u - 0.6f) / 0.4f * 256));
        fill(0, sy, SCREEN_W, 1, c);
    }
    for (int i = 0; i < 120; i++) {
        uint32_t h = hash32(i * 7 + 1);
        int x = (int)((h % 900) - fmodf(camx * 0.05f, 900.0f));
        if (x < 0) x += 900;
        int y = HUD_H + (int)((h >> 10) % 200) - (int)(camy * 0.15f) - 40;
        if (x >= SCREEN_W) continue;
        int tw = 120 + (int)(80 * sinf(time * (1 + (h & 3)) + i));
        px_set(x, y, cmix(0xFF0A0C18, (h & 64) ? 0xFFFFE8C8 : 0xFFC8D8FF, tw));
    }
    float mx = 370 - camx * 0.03f, my = HUD_H + 34 - camy * 0.12f;
    glow(mx, my, 60, 0xFF7080B0, 40);
    disc(mx, my, 11, 0xFFE8E4D2);
    disc(mx - 3, my - 2, 2.5f, 0xFFD0CCBA); disc(mx + 4, my + 3, 1.8f, 0xFFD0CCBA);
    /* two skyline layers */
    for (int layer = 0; layer < 2; layer++) {
        float par = layer ? 0.45f : 0.2f;
        uint32_t col = layer ? 0xFF0D1120 : 0xFF151A2E;
        int base = HUD_H + (int)(260 - camy * (layer ? 0.55f : 0.35f));
        float off = camx * par;
        int bi = (int)floorf(off / 34) - 1;
        for (int b = bi; b < bi + SCREEN_W / 20 + 4; b++) {
            uint32_t h = hash32((uint32_t)(b * 977 + layer * 13));
            int bw = 20 + (int)(h % 26);
            int bx = (int)(b * 34 - off);
            int bh = (layer ? 90 : 60) + (int)((h >> 8) % (layer ? 110 : 90));
            fill(bx, base - bh, bw, bh + 200, col);
            if ((h >> 20) % 3 == 0) fill(bx + bw / 2, base - bh - 10, 1, 10, col);
            for (int wy = base - bh + 5; wy < base; wy += 6)
                for (int wx = bx + 3; wx < bx + bw - 3; wx += 5) {
                    uint32_t hh = hash32((uint32_t)(wx * 31 + wy * 17 + layer));
                    if (hh % 11 == 0) fill(wx, wy, 2, 2, layer ? 0xFF5A4A30 : 0xFF3A3428);
                }
        }
    }
}

/* ------------------------------------------------------------------ */
/* character rig                                                       */
/* ------------------------------------------------------------------ */
const Look LOOK_OPERATOR = { 0xFFD9A27A, 0xFF2A2018, 0xFF4A5258, 0xFFFF8A2A, 0xFF3C4654, 0xFF8C7656,
                             0xFF2FD3C6, 0xFF3A4250, 0xFF141517, 0xFF1A1C20, 0 };
const Look LOOK_GUARD    = { 0xFFC99272, 0xFF1A1410, 0xFF5E6270, 0xFF000000, 0xFF555A66, 0xFFB8B4A8,
                             0xFFC8303A, 0xFF34415A, 0xFF1A1A1E, 0xFF1C1D20, 1 };
const Look LOOK_HEAVY    = { 0xFF2A2A2A, 0xFF101010, 0xFF3E4A2C, 0xFFFF3030, 0xFF4E5A36, 0xFF6A7448,
                             0xFF2A2A2A, 0xFF2C3222, 0xFF0E0F0C, 0xFF181A16, 2 };
const Look LOOK_HOSTAGE  = { 0xFFE0B08A, 0xFF5A3420, 0xFF5A3420, 0xFF000000, 0xFFF2C94C, 0xFFD8A830,
                             0xFFF2C94C, 0xFF3E4C6A, 0xFF3A2A20, 0xFF000000, 3 };

typedef struct { float x0, y0, x1, y1, r; uint32_t c; int hl; } Prim;
#define MAXPRIM 64
static Prim PR[MAXPRIM];
static int npr;
static void P_(float x0, float y0, float x1, float y1, float r, uint32_t c) {
    if (npr < MAXPRIM) PR[npr++] = (Prim){ x0, y0, x1, y1, r, c, 0 };
}
static void HL_(float x0, float y0, float x1, float y1, float r, uint32_t c) {  /* shading, no outline */
    if (npr < MAXPRIM) PR[npr++] = (Prim){ x0, y0, x1, y1, r, c, 1 };
}

void draw_rig(float cx, float gy, int dir, const Look *lk, const Pose *ps, int light,
              uint32_t flash, float sc, float *mx, float *my) {
    npr = 0;
    float m = ps->move, p = ps->walk, t = ps->t;
    int heavy = lk->kind == 2, hostage = lk->kind == 3;
    uint32_t back = 0;
#define L(c) cmul((c), light)
#define LB(c) cmul((c), light * 200 / 256)
    float Mx = 0, My = 0;

    if (hostage && !ps->secured) {
        float sh = sinf(t * 38) * 0.35f;
        float hx = sh, hy = -9;
        P_(hx, hy, 6, -2.5f, 2.5f, LB(lk->pants)); P_(6, -2.5f, -3, -1.6f, 2.1f, LB(lk->pants));
        P_(-4.5f, -1.2f, -2.0f, -1.2f, 1.6f, LB(lk->boots));
        P_(hx - 0.5f, hy - 10, -3, -27, 1.9f, LB(lk->top)); P_(-3, -27, -0.5f, -31, 1.6f, LB(lk->skin));
        P_(hx, hy, hx + 0.6f, hy - 10, 4.4f, L(lk->top));
        HL_(hx - 1.6f, hy - 1, hx - 1.0f, hy - 8, 0.9f, L(cmul(lk->top, 320)));
        P_(hx + 1.6f, hy - 9.4f, hx + 2.8f, hy - 7.8f, 1.0f, L(0xFFF8F4E8));
        P_(hx - 3.6f, hy + 0.5f, hx + 3.6f, hy + 0.5f, 0.9f, L(0xFF2A2420));
        P_(hx + 1.2f, hy - 15.5f, hx + 1.2f, hy - 15.5f, 4.4f, L(lk->skin));
        P_(hx - 0.2f, hy - 17.6f, hx + 1.8f, hy - 18.2f, 3.6f, L(lk->hair));
        P_(hx - 1.8f, hy - 16, hx - 1.4f, hy - 14, 1.4f, L(lk->hair));
        P_(hx + 3.6f, hy - 15.6f, hx + 3.6f, hy - 15.6f, 0.6f, L(0xFF101010));
        P_(hx + 3.0f, hy - 13.0f, hx + 4.2f, hy - 13.4f, 0.6f, L(0xFF6A2A20));        /* scared mouth */
        P_(hx + 4, -2.5f, -2, -0.8f, 2.5f, L(lk->pants));
        HL_(hx + 1.5f, -3.6f, 5, -3.4f, 0.8f, L(cmul(lk->pants, 320)));
        P_(-2, -0.8f, -5.2f, -0.6f, 1.7f, L(lk->boots));
        P_(hx + 0.6f, hy - 10, 5.5f, -26, 2.0f, L(lk->top)); P_(5.5f, -26, 2.5f, -31, 1.7f, L(lk->skin));
        (void)back;
    } else if (lk->kind == 0) {
        /* ---- operator: crouched combat stance, plate carrier, masked, scoped rifle ---- */
        float bob = -fabsf(sinf(p)) * 1.2f * m + sinf(t * 2.0f) * 0.35f * (1 - m);
        float hx = 0, hy = -16.2f + bob;
        float kick = ps->kick > 0 ? sinf(ps->kick * (float)M_PI) : 0;
        float shx = 3.4f - kick * 3.0f, shy = hy - 12.2f;
        float headx = shx + 1.9f, heady = shy - 5.2f, hr = 4.7f;
        if (ps->roll >= 0) { hy = -9; shx = 3.5f; shy = -17; headx = 6; heady = -21; }
        float kneeX[2], kneeY[2], footX[2], footY[2];
        for (int i = 0; i < 2; i++) {
            float ph = p + i * (float)M_PI;
            float a = sinf(ph) * 0.58f * m + (i ? 0.26f : -0.2f) * (1 - m);
            float bend = (0.2f + 0.6f * fmaxf(0, sinf(ph + 1.6f))) * m + 0.42f * (1 - m);
            if (i == 1 && kick > 0) { a = a * (1 - kick) + 1.5f * kick; bend = bend * (1 - kick) + 0.05f * kick; }
            if (ps->roll >= 0) { a = 1.9f; bend = 2.6f; }
            float b = a - bend;
            kneeX[i] = hx + 8.6f * sinf(a); kneeY[i] = hy + 8.6f * cosf(a);
            footX[i] = kneeX[i] + 8.6f * sinf(b); footY[i] = kneeY[i] + 8.6f * cosf(b);
        }
        uint32_t shirt = lk->top, plate = lk->top2, pouch = cmul(lk->top2, 200), dark = 0xFF1A1D22;
        /* back leg */
        P_(hx, hy, kneeX[0], kneeY[0], 2.9f, LB(lk->pants));
        P_(kneeX[0], kneeY[0], footX[0], footY[0], 2.5f, LB(lk->pants));
        P_(kneeX[0], kneeY[0], kneeX[0] + 0.6f, kneeY[0] + 0.4f, 2.0f, LB(dark));
        P_(footX[0] - 1, footY[0] - 0.6f, footX[0] + 3.2f, footY[0] - 0.6f, 2.1f, LB(lk->boots));
        /* rifle geometry */
        float aim = ps->aim, ga = (1 - aim) * 0.7f - ps->recoil * 0.2f;
        float ux = cosf(ga), uy = sinf(ga), nx = uy, ny = -ux;          /* n = "up" across the gun */
        float Gx = shx + 3.4f - ps->recoil * 2.4f, Gy = shy + 5.0f + (1 - aim) * 2.5f;
        /* back arm + backpack */
        P_(shx - 0.6f, shy + 1, shx - 0.8f, shy + 6.8f, 2.2f, LB(shirt));
        P_(shx - 0.8f, shy + 6.8f, Gx, Gy, 2.0f, LB(shirt));
        P_(hx - 4.0f, hy - 3, shx - 3.6f, shy + 2.5f, 2.8f, L(cmul(plate, 170)));
        /* torso: shirt, plate carrier, pouches, belt + holster */
        P_(hx, hy + 1, shx, shy, 5.2f, L(shirt));
        P_(hx + 1.3f, hy - 1.8f, shx + 1.0f, shy + 2.6f, 4.7f, L(plate));
        HL_(hx + 3.0f, hy - 3.0f, shx + 2.6f, shy + 2.0f, 1.1f, L(cmul(plate, 330)));
        for (int k = 0; k < 3; k++) P_(hx + 4.9f - k * 0.3f, hy - 2.6f - k * 3.0f, hx + 5.1f - k * 0.3f, hy - 1.6f - k * 3.0f, 1.25f, L(pouch));
        P_(hx - 4.0f, hy + 0.6f, hx + 4.4f, hy + 0.6f, 1.2f, L(dark));
        P_(hx - 1.6f, hy + 1.4f, hx - 2.0f, hy + 5.2f, 1.4f, L(dark));
        /* head: helmet, NVG mount, ear cup, mask, goggles */
        P_(headx, heady, headx, heady, hr, L(lk->skin));
        P_(headx + 1.4f, heady + 1.4f, headx + 2.0f, heady + 1.6f, 3.4f, L(0xFF20242A));
        P_(headx - 0.7f, heady - 1.9f, headx + 0.3f, heady - 2.1f, hr + 0.6f, L(lk->helm));
        HL_(headx - 1.6f, heady - 4.6f, headx + 1.4f, heady - 5.0f, 1.0f, L(cmul(lk->helm, 360)));
        P_(headx - 3.6f, heady - 1.2f, headx + 2.4f, heady - 1.2f, 0.55f, L(cmul(lk->helm, 150)));
        P_(headx + 2.8f, heady - 4.6f, headx + 4.8f, heady - 3.4f, 1.05f, L(dark));
        P_(headx - 1.4f, heady + 0.6f, headx - 1.4f, heady + 0.6f, 1.9f, L(dark));
        P_(headx - 1.6f, heady - 0.5f, headx + 1.8f, heady - 0.4f, 0.6f, L(dark));
        P_(headx + 2.0f, heady - 0.4f, headx + 4.7f, heady - 0.4f, 1.5f, lk->visor);
        HL_(headx + 3.4f, heady - 1.0f, headx + 4.4f, heady - 1.0f, 0.5f, 0xFFFFE0B0);
        /* front leg with knee pad */
        P_(hx, hy, kneeX[1], kneeY[1], 3.0f, L(lk->pants));
        HL_(hx + 1.2f, hy + 0.5f, kneeX[1] + 1.0f, kneeY[1] - 1.5f, 0.9f, L(cmul(lk->pants, 330)));
        P_(kneeX[1], kneeY[1], footX[1], footY[1], 2.6f, L(lk->pants));
        P_(kneeX[1], kneeY[1], kneeX[1] + 0.6f, kneeY[1] + 0.4f, 2.1f, L(dark));
        P_(footX[1] - 1, footY[1] - 0.6f, footX[1] + 3.4f, footY[1] - 0.6f, 2.2f, L(lk->boots));
        /* rifle: stock, body, optic, foregrip, suppressor */
        float len = 14.0f;
        P_(Gx - ux * 6, Gy - uy * 6 + 1.4f, Gx - ux * 2, Gy - uy * 2, 1.8f, L(lk->gun));
        P_(Gx - ux * 2, Gy - uy * 2, Gx + ux * len, Gy + uy * len, 1.45f, L(lk->gun));
        P_(Gx + ux * 10 + 0, Gy + uy * 10, Gx + ux * (len + 2.5f), Gy + uy * (len + 2.5f), 1.75f, L(0xFF101216));
        P_(Gx + ux * 3 + nx * 2.6f, Gy + uy * 3 + ny * 2.6f, Gx + ux * 7 + nx * 2.6f, Gy + uy * 7 + ny * 2.6f, 1.3f, L(dark));
        HL_(Gx + ux * 7.2f + nx * 2.6f, Gy + uy * 7.2f + ny * 2.6f, Gx + ux * 7.2f + nx * 2.6f, Gy + uy * 7.2f + ny * 2.6f, 0.7f, 0xFF7AE8FF);
        P_(Gx + ux * 2.5f, Gy + uy * 2.5f, Gx + ux * 3 - nx * 3.6f, Gy + uy * 3 - ny * 3.6f, 1.0f, L(lk->gun));
        P_(Gx + ux * 8.5f, Gy + uy * 8.5f, Gx + ux * 8.5f - nx * 3.2f, Gy + uy * 8.5f - ny * 3.2f, 0.9f, L(lk->gun));
        Mx = Gx + ux * (len + 3.5f); My = Gy + uy * (len + 3.5f);
        /* front arm, shoulder pad, gloves */
        float fgx = Gx + ux * 8.5f - nx * 1.5f, fgy = Gy + uy * 8.5f - ny * 1.5f;
        P_(shx + 0.6f, shy + 1, shx + 4.0f, shy + 7.2f, 2.4f, L(shirt));
        P_(shx + 4.0f, shy + 7.2f, fgx, fgy, 2.1f, L(shirt));
        P_(shx + 0.4f, shy + 1.2f, shx + 1.4f, shy + 3.0f, 2.7f, L(plate));
        P_(shx + 1.2f, shy + 2.0f, shx + 1.2f, shy + 2.0f, 1.0f, L(lk->accent));
        P_(fgx, fgy, fgx, fgy, 1.6f, L(dark));
        P_(Gx, Gy + 0.6f, Gx, Gy + 0.6f, 1.5f, L(dark));
    } else {
        /* ---- guards (hooded, masked), heavies (armoured, LMG), standing hostages ---- */
        float bob = -fabsf(sinf(p)) * 1.3f * m + sinf(t * 2.2f) * 0.3f * (1 - m);
        float hipY = -17 + bob;
        float kick = ps->kick > 0 ? sinf(ps->kick * (float)M_PI) : 0;
        float stun = ps->stunned ? sinf(t * 7) : 0;
        float hx = 0, hy = hipY;
        float lean = heavy ? 1.2f : 2.2f;
        float shx = lean - kick * 2.5f + stun * 1.4f, shy = hipY - (heavy ? 12.5f : 12.0f);
        float headx = shx + (heavy ? 1.0f : 1.6f) + stun, heady = shy - (heavy ? 5.0f : 5.6f);
        float hr = heavy ? 4.9f : 4.5f, tr = heavy ? 6.4f : 4.6f;
        if (ps->roll >= 0) { hy = -9; shx = 3; shy = -17; headx = 5; heady = -21; }
        float kneeX[2], kneeY[2], footX[2], footY[2];
        for (int i = 0; i < 2; i++) {
            float ph = p + i * (float)M_PI;
            float a = sinf(ph) * 0.55f * m + (i ? 0.16f : -0.14f) * (1 - m) * (heavy ? 1.6f : 1.0f);
            float bend = (0.15f + 0.6f * fmaxf(0, sinf(ph + 1.6f))) * m + (heavy ? 0.3f : 0.16f) * (1 - m);
            if (i == 1 && kick > 0) { a = a * (1 - kick) + 1.45f * kick; bend = bend * (1 - kick) + 0.1f * kick; }
            float b = a - bend;
            kneeX[i] = hx + 8.6f * sinf(a); kneeY[i] = hy + 8.6f * cosf(a);
            footX[i] = kneeX[i] + 8.6f * sinf(b); footY[i] = kneeY[i] + 8.6f * cosf(b);
        }
        float legr = heavy ? 3.3f : 2.6f;
        uint32_t topc = lk->top, top2 = lk->top2, dark = 0xFF16171A;
        /* back leg */
        P_(hx, hy, kneeX[0], kneeY[0], legr, LB(lk->pants));
        P_(kneeX[0], kneeY[0], footX[0], footY[0], legr - 0.3f, LB(lk->pants));
        if (heavy) P_(kneeX[0], kneeY[0] + 1, footX[0], footY[0] - 2, 2.4f, LB(top2));
        P_(footX[0] - 0.8f, footY[0] - 0.6f, footX[0] + 3.0f, footY[0] - 0.6f, heavy ? 2.3f : 1.9f, LB(lk->boots));
        if (!heavy && !hostage) P_(footX[0] - 0.8f, footY[0] + 0.6f, footX[0] + 3.0f, footY[0] + 0.6f, 0.6f, LB(0xFFD8D8D0));
        /* weapon geometry */
        float aim = ps->aim, ga = (1 - aim) * 0.75f - ps->recoil * 0.2f;
        if (ps->stunned) ga = 0.9f;
        float ux = cosf(ga), uy = sinf(ga), nx = uy, ny = -ux;
        float Gx = shx + 3.2f - ps->recoil * 2.2f, Gy = shy + 5.6f + (1 - aim) * 2.5f;
        int armed = !hostage;
        /* back arm */
        if (armed) {
            P_(shx - 0.5f, shy + 1, shx - 1.0f, shy + 7, heavy ? 2.8f : 2.0f, LB(topc));
            P_(shx - 1.0f, shy + 7, Gx, Gy, heavy ? 2.5f : 1.8f, LB(topc));
        } else {
            P_(shx - 0.5f, shy + 1, shx - 1.0f, shy + 8, 1.9f, LB(topc));
            P_(shx - 1.0f, shy + 8, shx - 0.2f, shy + 13.5f, 1.5f, LB(lk->skin));
        }
        /* torso */
        if (lk->kind == 1) {
            P_(hx - 2.2f, hy - 2, shx - 2.6f, shy + 1, 3.0f, L(cmul(topc, 170)));          /* hood hanging behind */
            P_(hx, hy + 1, shx, shy, tr, L(topc));                                         /* hoodie */
            P_(hx + 2.2f, hy - 1, shx + 2.2f, shy + 3, 1.6f, L(top2));                     /* open zip / shirt */
            HL_(hx - 1.2f, hy - 2, shx - 1.0f, shy + 2, 1.0f, L(cmul(topc, 330)));
            P_(hx + 1.0f, hy - 4.5f, hx + 4.5f, hy - 4.0f, 1.1f, L(cmul(topc, 160)));       /* pocket */
            P_(hx - 3.8f, hy + 0.5f, hx + 3.8f, hy + 0.5f, 1.0f, L(dark));
        } else if (heavy) {
            P_(hx, hy + 1, shx, shy, tr, L(topc));
            for (int k = 0; k < 3; k++) P_(hx + 1.5f, hy - 1.5f - k * 3.6f, hx + 4.6f, hy - 1.2f - k * 3.6f, 1.6f, L(k & 1 ? cmul(top2, 130) : top2));
            HL_(hx - 2.5f, hy - 2, shx - 2.0f, shy + 2.5f, 1.3f, L(cmul(topc, 330)));
            P_(hx - 4.6f, hy + 0.8f, hx + 4.6f, hy + 0.8f, 1.3f, L(dark));
        } else {   /* hostage standing (secured) */
            P_(hx, hy + 1, shx, shy, 4.3f, L(topc));
            P_(shx + 1.2f, shy + 0.8f, shx + 2.4f, shy + 2.4f, 1.0f, L(0xFFF8F4E8));       /* collar */
            HL_(hx - 1.5f, hy - 1, shx - 1.0f, shy + 2, 0.9f, L(cmul(topc, 320)));
            P_(hx - 3.6f, hy + 0.5f, hx + 3.6f, hy + 0.5f, 0.9f, L(0xFF2A2420));
        }
        /* head */
        P_(headx, heady, headx, heady, hr, L(lk->skin));
        if (lk->kind == 1) {
            P_(headx - 0.9f, heady - 0.8f, headx - 0.1f, heady - 1.6f, hr + 0.9f, L(cmul(topc, 230)));   /* hood up */
            P_(headx + 1.6f, heady - 0.2f, headx + 2.6f, heady - 0.2f, 3.0f, L(lk->skin));              /* face in hood */
            P_(headx + 0.6f, heady + 2.1f, headx + 4.2f, heady + 1.9f, 1.9f, L(lk->accent));            /* bandana */
            P_(headx + 2.0f, heady + 3.2f, headx + 3.6f, heady + 4.4f, 0.8f, L(lk->accent));
            P_(headx + 3.0f, heady - 1.0f, headx + 3.0f, heady - 1.0f, 0.75f, 0xFF0A0A0A);
            HL_(headx + 3.3f, heady - 1.3f, headx + 3.3f, heady - 1.3f, 0.35f, 0xFFF0F0F0);
            P_(headx + 2.0f, heady - 2.4f, headx + 4.0f, heady - 2.2f, 0.45f, L(0xFF2A1A12));           /* brow */
        } else if (heavy) {
            P_(headx - 0.4f, heady - 0.8f, headx + 0.6f, heady - 1.0f, hr + 1.0f, L(lk->helm));          /* full helmet */
            HL_(headx - 1.6f, heady - 4.4f, headx + 1.6f, heady - 4.9f, 1.1f, L(cmul(lk->helm, 330)));
            P_(headx + 1.8f, heady - 0.2f, headx + 5.0f, heady - 0.2f, 1.9f, L(0xFF141614));             /* face plate */
            P_(headx + 2.2f, heady - 0.6f, headx + 5.0f, heady - 0.6f, 0.7f, lk->visor);                  /* visor slit */
            P_(headx + 3.0f, heady + 2.6f, headx + 4.8f, heady + 2.6f, 1.0f, L(0xFF2A2A2A));             /* respirator */
            P_(shx - 0.8f, shy + 1.4f, shx + 1.6f, shy + 1.0f, 3.6f, L(top2));                           /* pauldron */
        } else {
            P_(headx - 0.8f, heady - 2.0f, headx + 1.2f, heady - 2.4f, hr - 0.4f, L(lk->hair));
            P_(headx - 2.6f, heady - 0.2f, headx - 2.0f, heady + 1.6f, 1.4f, L(lk->hair));
            P_(headx + 2.8f, heady - 0.3f, headx + 2.8f, heady - 0.3f, 0.6f, L(0xFF101010));
            P_(headx + 2.4f, heady + 2.4f, headx + 3.6f, heady + 2.2f, 0.4f, L(0xFF8A4A3A));
        }
        /* front leg */
        P_(hx, hy, kneeX[1], kneeY[1], legr + 0.1f, L(lk->pants));
        HL_(hx + 1.0f, hy + 0.5f, kneeX[1] + 0.8f, kneeY[1] - 1.5f, 0.8f, L(cmul(lk->pants, 320)));
        P_(kneeX[1], kneeY[1], footX[1], footY[1], legr - 0.2f, L(lk->pants));
        if (heavy) { P_(kneeX[1], kneeY[1] + 1, footX[1], footY[1] - 2, 2.5f, L(lk->top2)); P_(kneeX[1], kneeY[1], kneeX[1] + 0.5f, kneeY[1], 2.6f, L(dark)); }
        P_(footX[1] - 0.8f, footY[1] - 0.6f, footX[1] + 3.2f, footY[1] - 0.6f, heavy ? 2.4f : 2.0f, L(lk->boots));
        if (!heavy && !hostage) P_(footX[1] - 0.8f, footY[1] + 0.7f, footX[1] + 3.2f, footY[1] + 0.7f, 0.6f, L(0xFFD8D8D0));
        /* weapon + front arm */
        if (armed && !heavy) {   /* guard: wooden-furniture rifle with curved magazine */
            float len = 13.0f;
            P_(Gx - ux * 6, Gy - uy * 6 + 1.6f, Gx - ux * 1.5f, Gy - uy * 1.5f, 1.6f, L(0xFF6A4224));
            P_(Gx - ux * 1.5f, Gy - uy * 1.5f, Gx + ux * len, Gy + uy * len, 1.3f, L(lk->gun));
            P_(Gx + ux * 6, Gy + uy * 6, Gx + ux * 10, Gy + uy * 10, 1.5f, L(0xFF6A4224));
            P_(Gx + ux * 3.5f, Gy + uy * 3.5f, Gx + ux * 5.5f - nx * 4.2f, Gy + uy * 5.5f - ny * 4.2f, 1.1f, L(lk->gun));
            Mx = Gx + ux * (len + 1.5f); My = Gy + uy * (len + 1.5f);
            float fgx = Gx + ux * 7.5f, fgy = Gy + uy * 7.5f;
            P_(shx + 0.5f, shy + 1, shx + 3.5f, shy + 7.5f, 2.2f, L(topc));
            P_(shx + 3.5f, shy + 7.5f, fgx, fgy + 0.5f, 1.9f, L(topc));
            P_(fgx, fgy + 0.5f, fgx, fgy + 0.5f, 1.4f, L(lk->skin));
            P_(Gx, Gy + 0.6f, Gx, Gy + 0.6f, 1.3f, L(lk->skin));
        } else if (armed) {      /* heavy: light machine gun with drum */
            float len = 16.0f;
            P_(Gx - ux * 6, Gy - uy * 6 + 1.8f, Gx - ux * 2, Gy - uy * 2, 2.0f, L(lk->gun));
            P_(Gx - ux * 2, Gy - uy * 2, Gx + ux * 9, Gy + uy * 9, 2.1f, L(lk->gun));
            P_(Gx + ux * 9, Gy + uy * 9, Gx + ux * len, Gy + uy * len, 1.2f, L(0xFF2A2A2A));
            P_(Gx + ux * 13, Gy + uy * 13 - ny * 0.0f, Gx + ux * 13 - nx * 3.0f, Gy + uy * 13 - ny * 3.0f, 0.5f, L(0xFF2A2A2A));
            P_(Gx + ux * 3 - nx * 3.6f, Gy + uy * 3 - ny * 3.6f, Gx + ux * 3 - nx * 3.6f, Gy + uy * 3 - ny * 3.6f, 3.0f, L(0xFF3A3E36));
            HL_(Gx + ux * 2 - nx * 4.6f, Gy + uy * 2 - ny * 4.6f, Gx + ux * 2 - nx * 4.6f, Gy + uy * 2 - ny * 4.6f, 0.9f, L(0xFF8A9080));
            P_(Gx + ux * 1 + nx * 2.4f, Gy + uy * 1 + ny * 2.4f, Gx + ux * 6 + nx * 2.4f, Gy + uy * 6 + ny * 2.4f, 0.7f, L(lk->gun));
            Mx = Gx + ux * (len + 1.5f); My = Gy + uy * (len + 1.5f);
            float fgx = Gx + ux * 8, fgy = Gy + uy * 8;
            P_(shx + 0.5f, shy + 1, shx + 3.5f, shy + 7.5f, 2.9f, L(topc));
            P_(shx + 3.5f, shy + 7.5f, fgx, fgy + 0.5f, 2.5f, L(topc));
            P_(shx + 0.2f, shy + 0.8f, shx + 1.8f, shy + 2.2f, 3.4f, L(lk->top2));
            P_(fgx, fgy + 0.5f, fgx, fgy + 0.5f, 1.8f, L(dark));
        } else {
            P_(shx + 0.5f, shy + 1, shx + 1.5f, shy + 8, 1.9f, L(topc));
            P_(shx + 1.5f, shy + 8, shx + 2.5f, shy + 13, 1.6f, L(lk->skin));
        }
    }

    /* transform: fall (rotate about feet) or roll (spin about centre) */
    float th = 0, pvx = 0, pvy = 0, lift = 0;
    if (ps->fall > 0) { th = -ps->fall * (float)M_PI / 2; lift = -3.2f * ps->fall; }
    if (ps->roll >= 0) { th = ps->roll * 2 * (float)M_PI; pvx = 1; pvy = -11; }
    float cs = cosf(th), sn = sinf(th);
#define TX(x, y) (cx + dir * sc * (pvx + ((x) - pvx) * cs - ((y) - pvy) * sn))
#define TY(x, y) (gy + sc * (pvy + ((x) - pvx) * sn + ((y) - pvy) * cs + lift))
    uint32_t outline = 0xFF0A0B0E;
    for (int i = 0; i < npr; i++) {
        if (PR[i].hl) continue;
        capsule(TX(PR[i].x0, PR[i].y0), TY(PR[i].x0, PR[i].y0), TX(PR[i].x1, PR[i].y1), TY(PR[i].x1, PR[i].y1),
                (PR[i].r + 0.9f) * sc, outline);
    }
    for (int i = 0; i < npr; i++)
        capsule(TX(PR[i].x0, PR[i].y0), TY(PR[i].x0, PR[i].y0), TX(PR[i].x1, PR[i].y1), TY(PR[i].x1, PR[i].y1),
                PR[i].r * sc, flash ? (PR[i].hl ? flash : flash) : PR[i].c);
    if (mx) { *mx = TX(Mx, My); *my = TY(Mx, My); }
#undef TX
#undef TY
#undef L
#undef LB
}

/* ------------------------------------------------------------------ */
/* doors (drawn live)                                                  */
/* ------------------------------------------------------------------ */
void draw_door(int sx, int g, float open, int dir, int light, int broken) {
    uint32_t c = cmul(TH->door, light), cd = cmul(TH->door_dark, light);
    int top = g - DOOR_H + 1;
    if (open <= 0.001f) {
        fill(sx + 3, top, 10, DOOR_H - 1, c);
        fill(sx + 3, top, 1, DOOR_H - 1, cmul(c, 290));
        fill(sx + 12, top, 1, DOOR_H - 1, cd);
        fill(sx + 5, top + 4, 6, 20, cd); fill(sx + 6, top + 5, 4, 18, c);
        fill(sx + 5, top + 30, 6, 22, cd); fill(sx + 6, top + 31, 4, 20, c);
        fill(sx + 10, top + 28, 2, 3, cmul(0xFFE0C060, light));
        return;
    }
    float o = open > 1 ? 1 : open;
    int w = (int)(10 * (1 - o) + 3 * o + 0.5f);
    float ov = open > 1 ? (open - 1) * 4 : 0;
    w += (int)ov;
    int x0 = dir > 0 ? sx + 13 - w : sx + 3;
    if (broken) { top += 2; }
    fill(x0, top, w, DOOR_H - 1, cmul(cd, 230));
    fill(dir > 0 ? x0 : x0 + w - 1, top, 1, DOOR_H - 1, cmul(c, 260));
    fill(x0, g - 1, w + 4, 1, 0xFF0A0A0C);
}

/* ------------------------------------------------------------------ */
/* minimap (briefing / pause / operation select)                       */
/* ------------------------------------------------------------------ */
void draw_minimap(int lvl, int x, int y, int cell) {
    const LevelDef *L = &LEVELS[lvl];
    int w = 0, act[NFLOORS], n = 0;
    for (int f = 0; f < NFLOORS; f++) {
        int len = (int)strlen(L->rows[f]); if (len > w) w = len;
        act[f] = 0;
        for (int k = 0; k < len; k++) if (L->rows[f][k] != '#') act[f] = 1;
        n += act[f];
    }
    int row = 0;
    for (int f = 0; f < NFLOORS; f++) {
        if (!act[f]) continue;
        int len = (int)strlen(L->rows[f]);
        int yy = y + row * (cell * 3 + 3);
        fill(x - 2, yy - 2, w * cell + 4, cell * 3 + 4, 0xFF0A0C10);
        for (int k = 0; k < w; k++) {
            char ch = k < len ? L->rows[f][k] : '#';
            uint32_t c = ch == '#' ? 0xFF2A2E36 : ch == 'D' ? 0xFFB07A40 : ch == 'S' ? 0xFF2F9A5E : 0xFF59606C;
            fill(x + k * cell, yy, cell, cell * 3, c);
            if (ch == 'D') fill(x + k * cell, yy, cell, cell, 0xFF59606C);
            if (ch == 'H') disc(x + k * cell + cell / 2.0f, yy + cell * 2.0f, cell * 0.6f, 0xFFF2C94C);
            if (ch == 'P') disc(x + k * cell + cell / 2.0f, yy + cell * 2.0f, cell * 0.6f, 0xFF2FD3C6);
        }
        row++;
    }
    (void)n;
}

/* ------------------------------------------------------------------ */
/* pixel-art sprites (generated from tools/sprites.py)                 */
/* ------------------------------------------------------------------ */
void draw_sprite(int kind, int frame, float cx, float gy, int dir, int light, uint32_t flash, int sc,
                 float *mx, float *my) {
    const SprFrame *tab; const uint32_t *pal; const uint8_t *emi; int npal;
    switch (kind) {
    case 0: tab = SPR_op; pal = SPR_PAL_op; emi = SPR_EMI_op; npal = SPR_NPAL_op; break;
    case 1: tab = SPR_gd; pal = SPR_PAL_gd; emi = SPR_EMI_gd; npal = SPR_NPAL_gd; break;
    case 2: tab = SPR_hv; pal = SPR_PAL_hv; emi = SPR_EMI_hv; npal = SPR_NPAL_hv; break;
    default: tab = SPR_hs; pal = SPR_PAL_hs; emi = SPR_EMI_hs; npal = SPR_NPAL_hs; break;
    }
    const SprFrame *f = &tab[frame];
    if (!f->px) f = &tab[kind == 3 ? SF_KNEEL0 : SF_IDLE0];
    uint32_t lp[64];
    for (int i = 1; i < npal && i < 64; i++) lp[i] = flash ? flash : emi[i] ? pal[i] : cmul(pal[i], light);
    int bx = (int)floorf(cx), by = (int)floorf(gy) - SPR_FOOT * sc;
    for (int j = 0; j < f->h; j++) {
        const uint8_t *row = f->px + j * f->w;
        int y = by + (f->y0 + j) * sc;
        for (int i = 0; i < f->w; i++) {
            int c = row[i];
            if (!c) continue;
            int lx = f->x0 + i - SPR_AX;
            int x = dir > 0 ? bx + lx * sc : bx - (lx + 1) * sc + 1;
            if (sc == 1) px_set(x, y, lp[c]); else fill(x, y, sc, sc, lp[c]);
        }
    }
    if (mx) {
        int lx = f->mx - SPR_AX;
        *mx = dir > 0 ? bx + (lx + 1) * sc : bx - (lx + 1) * sc;
        *my = by + f->my * sc + sc * 0.5f;
    }
}
