/* BREACHLINE - procedural sound effects and a small music sequencer.
 * game code calls sfx()/music_set() from the main thread; the platform's audio
 * thread calls game_audio_mix(). They talk through a lock-free ring.
 *
 * Written to be cheap on a 333 MHz PSP: no sinf/expf/powf per sample. Sines come
 * from a lookup table and every envelope is a running multiply. */
#include "internal.h"

#define RATE    44100
#define NVOICE  16
#define QSIZE   64
#define SINN    1024

typedef struct { int id; float pan, vol; } Req;
static volatile Req queue[QSIZE];
static volatile unsigned q_w, q_r;
static volatile int music_track, music_req = -1;

void sfx(int id, float pan, float vol) {
    unsigned w = q_w;
    if (w - q_r >= QSIZE) return;
    queue[w % QSIZE].id = id; queue[w % QSIZE].pan = pan; queue[w % QSIZE].vol = vol;
    q_w = w + 1;
}
void music_set(int track) { music_req = track; }

/* ---- tables (filled once, on the first mix call) ---- */
static float SINT[SINN];
static float MTOF[128];
static int tables_ready;
static void init_tables(void) {
    for (int i = 0; i < SINN; i++) SINT[i] = sinf(i * 6.2831853f / SINN);
    for (int n = 0; n < 128; n++) MTOF[n] = 440.0f * powf(2.0f, (n - 69) / 12.0f);
    tables_ready = 1;
}
static inline float lsin(float ph) { return SINT[(int)(ph * SINN) & (SINN - 1)]; } /* ph in cycles */
static inline float wrap1(float ph) { return ph >= 1.0f ? ph - (int)ph : ph; }
static float decay_k(float rate) { return expf(-rate / RATE); }                    /* once per voice */

static uint32_t arng = 0xC0FFEE11u;
static inline float anoise(void) { arng ^= arng << 13; arng ^= arng >> 17; arng ^= arng << 5; return (float)(int32_t)arng * (1.0f / 2147483648.0f); }

/* ---- voices ---- */
typedef struct {
    int on, id, n, len, delay;          /* sample counters */
    float vol, pl, pr, pitch;
    float ph, ph2, f, fk;               /* oscillator phase, freq, freq decay toward base */
    float lp, lp2, hp, cut, cutk;
    float e[4], k[4];                   /* envelopes and their per-sample decay */
} Voice;
static Voice V[NVOICE];

static const float DUR[SFX_COUNT] = { 0.45f, 0.40f, 0.55f, 0.45f, 0.70f, 0.12f, 0.35f, 0.10f,
                                      0.12f, 0.06f, 0.05f, 0.30f, 0.20f, 0.06f, 0.14f, 0.18f,
                                      0.30f, 0.09f, 0.20f, 0.10f, 0.25f, 0.30f };

static void start(int id, float pan, float vol, float delay, float pitch) {
    int best = 0, bn = -1;
    for (int i = 0; i < NVOICE; i++) {
        if (!V[i].on) { best = i; bn = 1 << 30; break; }
        if (V[i].n > bn) { bn = V[i].n; best = i; }
    }
    Voice *v = &V[best];
    memset(v, 0, sizeof *v);
    v->on = 1; v->id = id; v->vol = vol; v->pitch = pitch;
    v->pl = (0.5f - pan * 0.4f) * 2; v->pr = (0.5f + pan * 0.4f) * 2;
    v->delay = (int)(delay * RATE);
    v->len = (int)(DUR[id] * RATE);
    for (int i = 0; i < 4; i++) { v->e[i] = 1; v->k[i] = 1; }
    switch (id) {
    case SFX_SHOT: case SFX_ESHOT: case SFX_HEAVYSHOT:
        v->cut = id == SFX_SHOT ? 0.55f : id == SFX_ESHOT ? 0.35f : 0.28f;
        v->cutk = decay_k(18);
        v->k[0] = decay_k(42); v->k[1] = decay_k(9); v->k[2] = decay_k(22);
        v->f = (id == SFX_HEAVYSHOT ? 52 : 70) * pitch;
        break;
    case SFX_KICK:  v->k[0] = decay_k(9); v->k[1] = decay_k(60); v->k[2] = decay_k(18); break;
    case SFX_DOOR:  v->k[0] = decay_k(6); v->k[1] = decay_k(7); v->f = 95 * pitch; break;
    case SFX_HIT:   v->k[0] = decay_k(30); v->k[1] = decay_k(28); v->f = 160 * pitch; break;
    case SFX_BODY:  v->k[0] = decay_k(30); v->k[1] = decay_k(14); v->f = 60 * pitch; break;
    case SFX_HURT:  v->k[0] = decay_k(10); v->k[1] = decay_k(8); break;
    case SFX_MAGOUT: case SFX_MAGIN: case SFX_DRY: case SFX_CASING: {
        float d = id == SFX_CASING ? 45 : id == SFX_DRY ? 120 : 70;
        v->k[0] = decay_k(d); v->k[1] = decay_k(d * 1.3f);
        v->f = (id == SFX_CASING ? 4200 : 1800) * pitch;
    } break;
    case SFX_STEP:  v->k[0] = decay_k(70); break;
    case SFX_SECURE: case SFX_UI: case SFX_UI_OK: case SFX_CLEAR: case SFX_STAR:
        v->k[0] = decay_k(id == SFX_CLEAR ? 7 : 14);
        v->f = (id == SFX_UI ? 880 : id == SFX_STAR ? 1320 : 660) * pitch;
        break;
    case SFX_FAIL:  v->k[0] = decay_k(5); v->f = 220 * pitch; break;
    case SFX_ALERT: v->k[0] = decay_k(25); break;
    default: break;
    }
}

static void handle_req(int id, float pan, float vol) {
    float pr = 0.92f + 0.16f * (anoise() * 0.5f + 0.5f);
    static const float CLEAR_P[4] = { 1, 1.26f, 1.5f, 2.0f }, FAIL_P[3] = { 1, 0.84f, 0.67f };
    switch (id) {
    case SFX_MAGOUT: start(SFX_MAGOUT, pan, vol, 0, 1); start(SFX_MAGIN, pan, vol, 0.55f, 1); break;
    case SFX_SECURE: start(SFX_SECURE, pan, vol, 0, 1); start(SFX_SECURE, pan, vol, 0.11f, 1.5f); break;
    case SFX_CLEAR:  for (int k = 0; k < 4; k++) start(SFX_CLEAR, 0, vol, k * 0.11f, CLEAR_P[k]); break;
    case SFX_FAIL:   for (int k = 0; k < 3; k++) start(SFX_FAIL, 0, vol, k * 0.2f, FAIL_P[k]); break;
    case SFX_UI_OK:  start(SFX_UI_OK, 0, vol, 0, 1); start(SFX_UI_OK, 0, vol, 0.06f, 1.5f); break;
    case SFX_KICK:   start(SFX_KICK, pan, vol, 0, pr); start(SFX_DOOR, pan, vol * 0.8f, 0.02f, pr); break;
    default:         start(id, pan, vol, 0, pr); break;
    }
}

static float voice_sample(Voice *v) {
    const float inv = 1.0f / RATE;
    float s = 0;
    switch (v->id) {
    case SFX_SHOT: case SFX_ESHOT: case SFX_HEAVYSHOT: {
        float cut = v->cut + 0.03f;
        v->lp += (anoise() - v->lp) * cut;
        v->lp2 += (v->lp - v->lp2) * (cut * 0.6f + 0.02f);
        v->ph = wrap1(v->ph + v->f * inv);
        s = v->lp * v->e[0] * 2.2f + v->lp2 * v->e[1] * 3.0f + lsin(v->ph) * v->e[2] * 0.9f;
        if (v->id == SFX_HEAVYSHOT) s *= 1.2f;
        v->cut *= v->cutk;
    } break;
    case SFX_KICK: {
        v->ph = wrap1(v->ph + (45 + 140 * v->e[2]) * v->pitch * inv);
        v->lp += (anoise() - v->lp) * 0.5f;
        s = lsin(v->ph) * v->e[0] * 1.1f + v->lp * v->e[1] * 0.9f;
    } break;
    case SFX_DOOR: {
        v->lp += (anoise() - v->lp) * 0.25f;
        v->ph2 = wrap1(v->ph2 + 34 * inv);
        v->ph = wrap1(v->ph + v->f * inv);
        s = v->lp * (0.6f + 0.4f * lsin(v->ph2)) * v->e[0] * 0.9f + lsin(v->ph) * v->e[1] * 0.4f;
    } break;
    case SFX_HIT: case SFX_BODY: {
        v->lp += (anoise() - v->lp) * 0.12f;
        v->ph = wrap1(v->ph + v->f * inv);
        s = v->lp * v->e[0] * 1.6f + lsin(v->ph) * v->e[1] * 0.6f;
    } break;
    case SFX_HURT: {
        float t = v->n * inv;
        v->ph = wrap1(v->ph + 70 * inv);
        v->ph2 = wrap1(v->ph2 + (330 - 200 * t) * inv);
        s = lsin(v->ph) * v->e[0] * 0.9f + (v->ph2 < 0.5f ? 0.18f : -0.18f) * v->e[1];
    } break;
    case SFX_MAGOUT: case SFX_MAGIN: case SFX_DRY: case SFX_CASING: {
        float x = anoise(), hp = x - v->hp; v->hp = x;
        v->ph = wrap1(v->ph + v->f * inv);
        s = hp * v->e[0] * (v->id == SFX_MAGIN ? 1.0f : 0.7f) + lsin(v->ph) * v->e[1] * 0.25f;
    } break;
    case SFX_STEP:
        v->lp += (anoise() - v->lp) * 0.08f;
        s = v->lp * v->e[0] * 1.4f;
        break;
    case SFX_ROLL: {
        v->lp += (anoise() - v->lp) * 0.06f;
        s = v->lp * lsin(0.5f * v->n / (float)v->len) * 1.1f;
    } break;
    case SFX_SECURE: case SFX_UI: case SFX_UI_OK: case SFX_CLEAR: case SFX_STAR: {
        v->ph = wrap1(v->ph + v->f * inv);
        float att = v->n < 220 ? v->n / 220.0f : 1.0f;
        s = (lsin(v->ph) + 0.3f * lsin(wrap1(v->ph * 2))) * att * v->e[0] * 0.45f;
    } break;
    case SFX_FAIL: {
        v->ph = wrap1(v->ph + v->f * inv);
        v->lp += ((v->ph < 0.5f ? 1.0f : -1.0f) - v->lp) * 0.08f;
        s = v->lp * v->e[0] * 0.35f;
    } break;
    case SFX_ALERT: {
        float t = v->n * inv;
        v->ph = wrap1(v->ph + (500 + 400 * t) * inv);
        s = lsin(v->ph) * v->e[0] * 0.25f;
    } break;
    case SFX_STAIRS: {
        int k = v->n % 3528;   /* a footfall every 0.08 s */
        v->lp += (anoise() - v->lp) * 0.1f;
        float env = k < 1300 ? 1.0f - k / 1300.0f : 0.0f;
        s = v->lp * env * env * 0.8f;
    } break;
    }
    for (int i = 0; i < 4; i++) { v->e[i] *= v->k[i]; if (v->e[i] < 1e-5f) v->e[i] = 0; }
    return s * v->vol;
}

/* ---- music: 16-step loop ---- */
static int   m_step, m_pos, m_step_len;
static float m_bass_ph, m_bass_lp, m_env_bass, m_env_hat, m_env_kick, m_kick_ph, m_hat_lp, m_pad_ph[3];
static const int BASS_MISSION[16] = { 33, 0, 33, 0, 36, 0, 33, 31, 33, 0, 33, 0, 38, 36, 35, 31 };
static const int BASS_TITLE[16]   = { 33, 0, 0, 0, 0, 0, 0, 0, 29, 0, 0, 0, 31, 0, 0, 0 };
static int m_note = 33;

static void music_tick(int track) {
    const int *bass = track == 1 ? BASS_TITLE : BASS_MISSION;
    if (bass[m_step]) { m_env_bass = 1; m_note = bass[m_step]; }
    if (track == 2) {
        if (m_step % 4 == 0) m_env_kick = 1;
        if (m_step % 2 == 1) m_env_hat = (m_step % 4 == 3) ? 0.9f : 0.5f;
    } else if (m_step % 8 == 4) m_env_hat = 0.4f;
}

static float music_sample(int track) {
    if (!track) return 0;
    if (++m_pos >= m_step_len) { m_pos = 0; m_step = (m_step + 1) & 15; music_tick(track); }
    float s = 0;
    if (m_env_bass > 1e-4f) {
        m_bass_ph = wrap1(m_bass_ph + MTOF[m_note] * (1.0f / RATE));
        float saw = 2 * m_bass_ph - 1;
        m_bass_lp += (saw - m_bass_lp) * (0.02f + 0.08f * m_env_bass);
        s += m_bass_lp * m_env_bass * 0.5f;
        m_env_bass *= track == 1 ? 0.99996f : 0.99988f;
    } else m_env_bass = 0;
    if (m_env_kick > 1e-4f) {
        m_kick_ph = wrap1(m_kick_ph + (45 + 90 * m_env_kick * m_env_kick) * (1.0f / RATE));
        s += lsin(m_kick_ph) * m_env_kick * 0.6f;
        m_env_kick *= 0.9993f;
    } else m_env_kick = 0;
    if (m_env_hat > 1e-4f) {
        float n = anoise(), hp = n - m_hat_lp;
        m_hat_lp += (n - m_hat_lp) * 0.3f;
        s += hp * m_env_hat * 0.12f;
        m_env_hat *= 0.9975f;
    } else m_env_hat = 0;
    if (track == 1) {
        static const int PAD[3] = { 57, 60, 64 };
        for (int k = 0; k < 3; k++) { m_pad_ph[k] = wrap1(m_pad_ph[k] + MTOF[PAD[k]] * (1.0f / RATE)); s += lsin(m_pad_ph[k]) * 0.035f; }
    }
    return s * 0.30f;
}

void game_audio_mix(int16_t *out, int frames) {
    if (!tables_ready) init_tables();
    while (q_r != q_w) {
        Req r = { queue[q_r % QSIZE].id, queue[q_r % QSIZE].pan, queue[q_r % QSIZE].vol };
        q_r = q_r + 1;
        if (r.id >= 0 && r.id < SFX_COUNT) handle_req(r.id, r.pan, r.vol);
    }
    if (music_req >= 0) {
        music_track = music_req; music_req = -1;
        m_step = 15; m_pos = 0;
        m_step_len = (int)(RATE * 60.0f / (music_track == 1 ? 76 : 104) / 4);
    }
    int track = music_track;
    for (int i = 0; i < frames; i++) {
        float l = 0, r = 0;
        for (int k = 0; k < NVOICE; k++) {
            Voice *v = &V[k];
            if (!v->on) continue;
            if (v->delay > 0) { v->delay--; continue; }
            float s = voice_sample(v);
            if (++v->n >= v->len) v->on = 0;
            l += s * v->pl; r += s * v->pr;
        }
        float m = music_sample(track);
        l = l * 0.55f + m; r = r * 0.55f + m;
        if (l > 1) l = 1; else if (l < -1) l = -1;    /* cheap hard clip instead of a divide */
        if (r > 1) r = 1; else if (r < -1) r = -1;
        out[i * 2] = (int16_t)(l * 30000);
        out[i * 2 + 1] = (int16_t)(r * 30000);
    }
}
