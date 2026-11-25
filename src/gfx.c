/* BREACHLINE - drawing primitives and bitmap font */
#include "internal.h"

Target *TG;
int TOX, TOY;
static int CX0, CY0, CX1, CY1;

void tg_set(Target *t, int ox, int oy) {
    TG = t; TOX = ox; TOY = oy;
    CX0 = 0; CY0 = 0; CX1 = t->w; CY1 = t->h;
}
void tg_clip(int x0, int y0, int x1, int y1) {
    CX0 = x0 < 0 ? 0 : x0; CY0 = y0 < 0 ? 0 : y0;
    CX1 = x1 > TG->w ? TG->w : x1; CY1 = y1 > TG->h ? TG->h : y1;
}

int tg_clip_y1(void) { return CY1; }

uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

uint32_t cmul(uint32_t c, int k) {
    int r = (int)(((c >> 16) & 255) * k) >> 8, g = (int)(((c >> 8) & 255) * k) >> 8, b = (int)((c & 255) * k) >> 8;
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}
uint32_t cmix(uint32_t a, uint32_t b, int t) {
    if (t <= 0) return a | 0xFF000000u;
    if (t >= 256) return b | 0xFF000000u;
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return 0xFF000000u | (uint32_t)(ar + (((br - ar) * t) >> 8)) << 16 |
           (uint32_t)(ag + (((bg - ag) * t) >> 8)) << 8 | (uint32_t)(ab + (((bb - ab) * t) >> 8));
}
uint32_t cadd(uint32_t d, uint32_t s, int a) {
    int r = ((d >> 16) & 255) + ((((s >> 16) & 255) * a) >> 8);
    int g = ((d >> 8) & 255) + ((((s >> 8) & 255) * a) >> 8);
    int b = (d & 255) + (((s & 255) * a) >> 8);
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return 0xFF000000u | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}

void px_set(int x, int y, uint32_t c) {
    x += TOX; y += TOY;
    if (x < CX0 || y < CY0 || x >= CX1 || y >= CY1) return;
    TG->px[y * TG->w + x] = c;
}

#define CLIPRECT                                             \
    x += TOX; y += TOY;                                      \
    int x1 = x + w, y1 = y + h;                              \
    if (x < CX0) x = CX0; if (y < CY0) y = CY0;              \
    if (x1 > CX1) x1 = CX1; if (y1 > CY1) y1 = CY1;          \
    if (x >= x1 || y >= y1) return;

void fill(int x, int y, int w, int h, uint32_t c) {
    CLIPRECT
    for (int j = y; j < y1; j++) {
        uint32_t *r = TG->px + j * TG->w;
        for (int i = x; i < x1; i++) r[i] = c;
    }
}
void fill_a(int x, int y, int w, int h, uint32_t c, int a) {
    CLIPRECT
    for (int j = y; j < y1; j++) {
        uint32_t *r = TG->px + j * TG->w;
        for (int i = x; i < x1; i++) r[i] = cmix(r[i], c, a);
    }
}
void fill_add(int x, int y, int w, int h, uint32_t c, int a) {
    CLIPRECT
    for (int j = y; j < y1; j++) {
        uint32_t *r = TG->px + j * TG->w;
        for (int i = x; i < x1; i++) r[i] = cadd(r[i], c, a);
    }
}
void vgrad(int x, int y, int w, int h, uint32_t top, uint32_t bot) {
    int y0 = y;
    CLIPRECT
    for (int j = y; j < y1; j++) {
        uint32_t c = cmix(top, bot, h > 1 ? (j - TOY - y0) * 256 / (h - 1) : 0);
        uint32_t *r = TG->px + j * TG->w;
        for (int i = x; i < x1; i++) r[i] = c;
    }
}

#define BBOX(cx, cy, r)                                                     \
    int bx0 = (int)floorf((cx) - (r)) + TOX, bx1 = (int)ceilf((cx) + (r)) + TOX; \
    int by0 = (int)floorf((cy) - (r)) + TOY, by1 = (int)ceilf((cy) + (r)) + TOY; \
    if (bx0 < CX0) bx0 = CX0; if (by0 < CY0) by0 = CY0;                     \
    if (bx1 > CX1) bx1 = CX1; if (by1 > CY1) by1 = CY1;

void disc(float cx, float cy, float r, uint32_t c) {
    BBOX(cx, cy, r)
    float r2 = r * r;
    for (int j = by0; j < by1; j++) {
        float dy = j - TOY + 0.5f - cy;
        for (int i = bx0; i < bx1; i++) {
            float dx = i - TOX + 0.5f - cx;
            if (dx * dx + dy * dy <= r2) TG->px[j * TG->w + i] = c;
        }
    }
}
void disc_a(float cx, float cy, float r, uint32_t c, int a) {
    BBOX(cx, cy, r)
    float r2 = r * r;
    for (int j = by0; j < by1; j++) {
        float dy = j - TOY + 0.5f - cy;
        for (int i = bx0; i < bx1; i++) {
            float dx = i - TOX + 0.5f - cx;
            if (dx * dx + dy * dy <= r2) TG->px[j * TG->w + i] = cmix(TG->px[j * TG->w + i], c, a);
        }
    }
}
void disc_cap(float cx, float cy, float r, uint32_t c, float ymax) {
    BBOX(cx, cy, r)
    float r2 = r * r;
    for (int j = by0; j < by1; j++) {
        float yy = j - TOY + 0.5f;
        if (yy > ymax) break;
        float dy = yy - cy;
        for (int i = bx0; i < bx1; i++) {
            float dx = i - TOX + 0.5f - cx;
            if (dx * dx + dy * dy <= r2) TG->px[j * TG->w + i] = c;
        }
    }
}
void ring(float cx, float cy, float r, uint32_t c) {
    BBOX(cx, cy, r + 1)
    float ro = (r + 0.5f) * (r + 0.5f), ri = (r - 0.5f) * (r - 0.5f);
    for (int j = by0; j < by1; j++) {
        float dy = j - TOY + 0.5f - cy;
        for (int i = bx0; i < bx1; i++) {
            float dx = i - TOX + 0.5f - cx, d = dx * dx + dy * dy;
            if (d <= ro && d >= ri) TG->px[j * TG->w + i] = c;
        }
    }
}

void capsule(float x0, float y0, float x1, float y1, float r, uint32_t c) {
    int bx0 = (int)floorf(fminf(x0, x1) - r) + TOX, bx1 = (int)ceilf(fmaxf(x0, x1) + r) + TOX;
    int by0 = (int)floorf(fminf(y0, y1) - r) + TOY, by1 = (int)ceilf(fmaxf(y0, y1) + r) + TOY;
    if (bx0 < CX0) bx0 = CX0; if (by0 < CY0) by0 = CY0;
    if (bx1 > CX1) bx1 = CX1; if (by1 > CY1) by1 = CY1;
    float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy, r2 = r * r;
    float inv = L2 > 1e-6f ? 1.0f / L2 : 0;
    for (int j = by0; j < by1; j++) {
        float py = j - TOY + 0.5f;
        for (int i = bx0; i < bx1; i++) {
            float px = i - TOX + 0.5f;
            float t = ((px - x0) * dx + (py - y0) * dy) * inv;
            if (t < 0) t = 0; else if (t > 1) t = 1;
            float ex = x0 + t * dx - px, ey = y0 + t * dy - py;
            if (ex * ex + ey * ey <= r2) TG->px[j * TG->w + i] = c;
        }
    }
}

static void line_gen(float x0, float y0, float x1, float y1, uint32_t c, int a, int add) {
    float dx = x1 - x0, dy = y1 - y0;
    int n = (int)(fmaxf(fabsf(dx), fabsf(dy))) + 1;
    for (int k = 0; k <= n; k++) {
        float t = (float)k / n;
        int x = (int)floorf(x0 + dx * t) + TOX, y = (int)floorf(y0 + dy * t) + TOY;
        if (x < CX0 || y < CY0 || x >= CX1 || y >= CY1) continue;
        uint32_t *p = TG->px + y * TG->w + x;
        int aa = add == 2 ? a * k / n : a; /* fading tracer */
        *p = add ? cadd(*p, c, aa) : cmix(*p, c, aa);
    }
}
void line_a(float x0, float y0, float x1, float y1, uint32_t c, int a) { line_gen(x0, y0, x1, y1, c, a, 0); }
void line_add(float x0, float y0, float x1, float y1, uint32_t c, int a) { line_gen(x0, y0, x1, y1, c, a, 2); }

/* glow: additive (1-d)^2 falloff. d^2 indexes a table, so no sqrt per pixel */
static uint16_t GLOW_LUT[1025];
static int glow_ready;
void glow(float cx, float cy, float r, uint32_t c, int a) {
    if (!glow_ready) {
        for (int i = 0; i <= 1024; i++) { float d = sqrtf(i / 1024.0f); GLOW_LUT[i] = (uint16_t)((1 - d) * (1 - d) * 256); }
        glow_ready = 1;
    }
    BBOX(cx, cy, r)
    float k = 1024.0f / (r * r);
    int cr = (c >> 16) & 255, cg = (c >> 8) & 255, cb = c & 255;
    for (int j = by0; j < by1; j++) {
        float dy = j - TOY + 0.5f - cy, dy2 = dy * dy;
        uint32_t *row = TG->px + j * TG->w;
        for (int i = bx0; i < bx1; i++) {
            float dx = i - TOX + 0.5f - cx;
            int idx = (int)((dx * dx + dy2) * k);
            if (idx >= 1024) continue;
            int w = (a * GLOW_LUT[idx]) >> 8;
            if (w <= 0) continue;
            uint32_t d = row[i];
            int r_ = ((d >> 16) & 255) + ((cr * w) >> 8), g_ = ((d >> 8) & 255) + ((cg * w) >> 8), b_ = (d & 255) + ((cb * w) >> 8);
            if (r_ > 255) r_ = 255; if (g_ > 255) g_ = 255; if (b_ > 255) b_ = 255;
            row[i] = 0xFF000000u | (uint32_t)r_ << 16 | (uint32_t)g_ << 8 | (uint32_t)b_;
        }
    }
}

/* ---- 5x7 font (bit 4 = leftmost column) ---- */
static const uint8_t FONT[][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, {0x11,0x11,0x19,0x15,0x13,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
};
static const uint8_t DIGITS[][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
};
typedef struct { char c; uint8_t g[7]; } Sym;
static const Sym SYMS[] = {
    {'.', {0,0,0,0,0,0x0C,0x0C}},             {':', {0,0x0C,0x0C,0,0x0C,0x0C,0}},
    {'!', {0x04,0x04,0x04,0x04,0x04,0,0x04}},  {'?', {0x0E,0x11,0x01,0x02,0x04,0,0x04}},
    {'-', {0,0,0,0x1F,0,0,0}},                {'/', {0x01,0x01,0x02,0x04,0x08,0x10,0x10}},
    {'%', {0x18,0x19,0x02,0x04,0x08,0x13,0x03}}, {'\'',{0x04,0x04,0x08,0,0,0,0}},
    {'>', {0x08,0x04,0x02,0x01,0x02,0x04,0x08}}, {'<', {0x02,0x04,0x08,0x10,0x08,0x04,0x02}},
    {'(', {0x02,0x04,0x08,0x08,0x08,0x04,0x02}}, {')', {0x08,0x04,0x02,0x02,0x02,0x04,0x08}},
    {'+', {0,0x04,0x04,0x1F,0x04,0x04,0}},     {'=', {0,0,0x1F,0,0x1F,0,0}},
    {',', {0,0,0,0,0x0C,0x04,0x08}},           {'*', {0x04,0x04,0x1F,0x0E,0x0E,0x1B,0x11}},
    {'#', {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0}},  {'_', {0,0,0,0,0,0,0x1F}},
    /* PSP face-button icons: \1 square \2 cross \3 circle \4 triangle */
    {'\1',{0,0x1F,0x11,0x11,0x11,0x1F,0}},     {'\2',{0,0x11,0x0A,0x04,0x0A,0x11,0}},
    {'\3',{0,0x0E,0x11,0x11,0x11,0x0E,0}},     {'\4',{0,0x04,0x0A,0x0A,0x11,0x1F,0}},
};
static const uint32_t ICON_COL[5] = { 0, 0xFFF07AC8, 0xFF7AA8FF, 0xFFFF6B6B, 0xFF5EE08A };

static const uint8_t *glyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= 'A' && c <= 'Z') return FONT[c - 'A'];
    if (c >= '0' && c <= '9') return DIGITS[c - '0'];
    for (unsigned i = 0; i < sizeof(SYMS) / sizeof(SYMS[0]); i++)
        if (SYMS[i].c == c) return SYMS[i].g;
    return NULL;
}

int text_w(const char *s, int sc) { int n = (int)strlen(s); return n ? n * 6 * sc - sc : 0; }

static void text_gen(int x, int y, const char *s, uint32_t top, uint32_t bot, int sc, int icons) {
    for (; *s; s++, x += 6 * sc) {
        const uint8_t *g = glyph(*s);
        if (!g) continue;
        int is_icon = *s >= 1 && *s <= 4;
        for (int r = 0; r < 7; r++) {
            uint32_t c = (icons && is_icon) ? ICON_COL[(int)*s] : (top == bot ? top : cmix(top, bot, r * 256 / 6));
            for (int b = 0; b < 5; b++)
                if (g[r] & (0x10 >> b)) fill(x + b * sc, y + r * sc, sc, sc, c);
        }
    }
}
void text_raw(int x, int y, const char *s, uint32_t c, int sc) { text_gen(x, y, s, c, c, sc, 1); }
void text(int x, int y, const char *s, uint32_t c, int sc) {
    static const int OX[8] = {-1,0,1,-1,1,-1,0,1}, OY[8] = {-1,-1,-1,0,0,1,1,1};
    text_gen(x + sc, y + sc, s, 0xFF000000, 0xFF000000, sc, 0);
    for (int k = 0; k < 8; k++) text_gen(x + OX[k], y + OY[k], s, 0xFF06070A, 0xFF06070A, sc, 0);
    text_gen(x, y, s, c, c, sc, 1);
}
void text_c(int cx, int y, const char *s, uint32_t c, int sc) { text(cx - text_w(s, sc) / 2, y, s, c, sc); }
void text_grad(int x, int y, const char *s, uint32_t top, uint32_t bot, int sc) {
    static const int OX[8] = {-1,0,1,-1,1,-1,0,1}, OY[8] = {-1,-1,-1,0,0,1,1,1};
    text_gen(x + sc / 2 + 1, y + sc, s, 0xFF000000, 0xFF000000, sc, 0);
    for (int k = 0; k < 8; k++) text_gen(x + OX[k] * (sc > 2 ? 2 : 1), y + OY[k] * (sc > 2 ? 2 : 1), s, 0xFF06070A, 0xFF06070A, sc, 0);
    text_gen(x, y, s, top, bot, sc, 0);
}
