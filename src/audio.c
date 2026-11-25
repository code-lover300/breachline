/* BREACHLINE - procedural sound effects and a small music sequencer.
 * game code calls sfx()/music_set() from the main thread; the platform's audio
 * thread calls game_audio_mix(). They talk through a lock-free ring. */
#include "internal.h"

#define RATE    44100
#define NVOICE  24
#define QSIZE   64

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

/* ---- audio thread state ---- */
typedef struct {
    int on, id;
    float t, dur, vol, pan, pitch, delay;
    float ph, ph2, lp, lp2, hp;
} Voice;
static Voice V[NVOICE];
static uint32_t arng = 0xC0FFEE11u;
static inline float anoise(void) { arng ^= arng << 13; arng ^= arng >> 17; arng ^= arng << 5; return (float)(int32_t)arng / 2147483648.0f; }

static void start(int id, float pan, float vol, float delay, float pitch) {
    int best = 0; float bt = -1;
    for (int i = 0; i < NVOICE; i++) {
        if (!V[i].on) { best = i; bt = 1e9f; break; }
        if (V[i].t > bt) { bt = V[i].t; best = i; }
    }
    Voice *v = &V[best];
    memset(v, 0, sizeof *v);
    static const float DUR[SFX_COUNT] = { 0.45f, 0.40f, 0.55f, 0.45f, 0.70f, 0.12f, 0.35f, 0.10f,
                                          0.12f, 0.06f, 0.05f, 0.30f, 0.20f, 0.06f, 0.14f, 0.18f,
                                          0.30f, 0.09f, 0.20f, 0.10f, 0.25f, 0.30f };
    v->on = 1; v->id = id; v->pan = pan; v->vol = vol; v->delay = delay; v->pitch = pitch;
    v->dur = DUR[id];
}

static void handle_req(int id, float pan, float vol) {
    float pr = 0.92f + 0.16f * (anoise() * 0.5f + 0.5f);
    switch (id) {
    case SFX_MAGOUT: start(SFX_MAGOUT, pan, vol, 0, 1); start(SFX_MAGIN, pan, vol, 0.55f, 1); break;
    case SFX_SECURE: start(SFX_SECURE, pan, vol, 0, 1); start(SFX_SECURE, pan, vol, 0.11f, 1.5f); break;
    case SFX_CLEAR:
        for (int k = 0; k < 4; k++) start(SFX_CLEAR, 0, vol, k * 0.11f, (float[]){ 1, 1.26f, 1.5f, 2.0f }[k]);
        break;
    case SFX_FAIL:
        for (int k = 0; k < 3; k++) start(SFX_FAIL, 0, vol, k * 0.2f, (float[]){ 1, 0.84f, 0.67f }[k]);
        break;
    case SFX_UI_OK: start(SFX_UI_OK, 0, vol, 0, 1); start(SFX_UI_OK, 0, vol, 0.06f, 1.5f); break;
    case SFX_KICK: start(SFX_KICK, pan, vol, 0, pr); start(SFX_DOOR, pan, vol * 0.8f, 0.02f, pr); break;
    default: start(id, pan, vol, 0, pr); break;
    }
}

static float voice_sample(Voice *v) {
    float t = v->t, s = 0, p = v->pitch;
    switch (v->id) {
    case SFX_SHOT: case SFX_ESHOT: case SFX_HEAVYSHOT: {
        float bright = v->id == SFX_SHOT ? 0.55f : v->id == SFX_ESHOT ? 0.35f : 0.28f;
        float cut = bright * expf(-t * 18) + 0.03f;
        v->lp += (anoise() - v->lp) * cut;
        v->lp2 += (v->lp - v->lp2) * (cut * 0.6f + 0.02f);
        float crack = v->lp * expf(-t * 42) * 2.2f;
        float body = v->lp2 * expf(-t * 9) * 3.0f;
        v->ph += (v->id == SFX_HEAVYSHOT ? 52 : 70) * p / RATE;
        float thump = sinf(v->ph * 6.2832f) * expf(-t * 22) * 0.9f;
        s = crack + body + thump;
        if (v->id == SFX_HEAVYSHOT) s *= 1.2f;
    } break;
    case SFX_KICK: {
        float f = 45 + 140 * expf(-t * 18);
        v->ph += f * p / RATE;
        s = sinf(v->ph * 6.2832f) * expf(-t * 9) * 1.1f;
        v->lp += (anoise() - v->lp) * 0.5f;
        s += v->lp * expf(-t * 60) * 0.9f;
    } break;
    case SFX_DOOR: {
        v->lp += (anoise() - v->lp) * 0.25f;
        float rattle = 0.6f + 0.4f * sinf(t * 2 * 3.14159f * 34);
        s = v->lp * rattle * expf(-t * 6) * 0.9f;
        v->ph += 95 * p / RATE;
        s += sinf(v->ph * 6.2832f) * expf(-t * 7) * 0.4f;
    } break;
    case SFX_HIT: case SFX_BODY: {
        v->lp += (anoise() - v->lp) * 0.12f;
        v->ph += (v->id == SFX_BODY ? 60 : 160) * p / RATE;
        s = v->lp * expf(-t * 30) * 1.6f + sinf(v->ph * 6.2832f) * expf(-t * (v->id == SFX_BODY ? 14 : 28)) * 0.6f;
    } break;
    case SFX_HURT: {
        v->ph += 70 / (float)RATE;
        v->ph2 += (330 - 200 * t) / RATE;
        s = sinf(v->ph * 6.2832f) * expf(-t * 10) * 0.9f + ((fmodf(v->ph2, 1) < 0.5f) ? 0.18f : -0.18f) * expf(-t * 8);
    } break;
    case SFX_MAGOUT: case SFX_MAGIN: case SFX_DRY: case SFX_CASING: {
        float hpf = anoise();
        float hp = hpf - v->hp; v->hp = hpf;
        float decay = v->id == SFX_CASING ? 45 : v->id == SFX_DRY ? 120 : 70;
        s = hp * expf(-t * decay) * (v->id == SFX_MAGIN ? 1.0f : 0.7f);
        v->ph += (v->id == SFX_CASING ? 4200 : 1800) * p / RATE;
        s += sinf(v->ph * 6.2832f) * expf(-t * decay * 1.3f) * 0.25f;
    } break;
    case SFX_STEP: {
        v->lp += (anoise() - v->lp) * 0.08f;
        s = v->lp * expf(-t * 70) * 1.4f;
    } break;
    case SFX_ROLL: {
        v->lp += (anoise() - v->lp) * 0.06f;
        float env = sinf(fminf(t / v->dur, 1) * 3.14159f);
        s = v->lp * env * 1.1f;
    } break;
    case SFX_SECURE: case SFX_UI: case SFX_UI_OK: case SFX_CLEAR: case SFX_STAR: {
        float f = v->id == SFX_UI ? 880 : v->id == SFX_STAR ? 1320 : 660;
        v->ph += f * p / RATE;
        float env = fminf(t * 200, 1) * expf(-t * (v->id == SFX_CLEAR ? 7 : 14));
        s = (sinf(v->ph * 6.2832f) + 0.3f * sinf(v->ph * 12.566f)) * env * 0.45f;
    } break;
    case SFX_FAIL: {
        v->ph += 220 * p / RATE;
        float sq = fmodf(v->ph, 1) < 0.5f ? 1 : -1;
        v->lp += (sq - v->lp) * 0.08f;
        s = v->lp * expf(-t * 5) * 0.35f;
    } break;
    case SFX_ALERT: {
        v->ph += (500 + 400 * t) / RATE;
        s = sinf(v->ph * 6.2832f) * expf(-t * 25) * 0.25f;
    } break;
    case SFX_STAIRS: {
        float k = fmodf(t, 0.08f);
        v->lp += (anoise() - v->lp) * 0.1f;
        s = v->lp * expf(-k * 80) * 0.8f;
    } break;
    }
    return s * v->vol;
}

/* ---- music: 16-step loop ---- */
static int   m_step;
static float m_pos, m_bass_ph, m_bass_lp, m_env_bass, m_env_hat, m_env_kick, m_kick_ph, m_hat_lp, m_pad_ph[3];
static const int BASS_MISSION[16] = { 33, 0, 33, 0, 36, 0, 33, 31, 33, 0, 33, 0, 38, 36, 35, 31 };
static const int BASS_TITLE[16]   = { 33, 0, 0, 0, 0, 0, 0, 0, 29, 0, 0, 0, 31, 0, 0, 0 };
static float mtof(int n) { return 440.0f * powf(2.0f, (n - 69) / 12.0f); }

static void music_tick(int track) {
    const int *bass = track == 1 ? BASS_TITLE : BASS_MISSION;
    if (bass[m_step]) { m_env_bass = 1; m_bass_ph = m_bass_ph; }
    if (track == 2) {
        if (m_step % 4 == 0) m_env_kick = 1;
        if (m_step % 2 == 1) m_env_hat = (m_step % 4 == 3) ? 0.9f : 0.5f;
    } else if (m_step % 8 == 4) m_env_hat = 0.4f;
}

static float music_sample(int track) {
    if (!track) return 0;
    float bpm = track == 1 ? 76 : 104;
    float step_len = RATE * 60.0f / bpm / 4;
    m_pos += 1;
    if (m_pos >= step_len) { m_pos -= step_len; m_step = (m_step + 1) & 15; music_tick(track); }
    const int *bass = track == 1 ? BASS_TITLE : BASS_MISSION;
    int note = bass[m_step];
    float s = 0;
    if (note || m_env_bass > 0.01f) {
        int n = note ? note : 33;
        m_bass_ph += mtof(n) / RATE;
        float saw = 2 * fmodf(m_bass_ph, 1) - 1;
        m_bass_lp += (saw - m_bass_lp) * (0.02f + 0.08f * m_env_bass);
        s += m_bass_lp * m_env_bass * 0.5f;
        m_env_bass *= track == 1 ? 0.99996f : 0.99988f;
    }
    if (m_env_kick > 0.001f) {
        m_kick_ph += (45 + 90 * m_env_kick * m_env_kick) / RATE;
        s += sinf(m_kick_ph * 6.2832f) * m_env_kick * 0.6f;
        m_env_kick *= 0.9993f;
    }
    if (m_env_hat > 0.001f) {
        float n = anoise();
        float hp = n - m_hat_lp; m_hat_lp += (n - m_hat_lp) * 0.3f;
        s += hp * m_env_hat * 0.12f;
        m_env_hat *= 0.9975f;
    }
    if (track == 1) {   /* slow minor pad */
        static const int PAD[3] = { 57, 60, 64 };
        for (int k = 0; k < 3; k++) { m_pad_ph[k] += mtof(PAD[k]) / RATE; s += sinf(m_pad_ph[k] * 6.2832f) * 0.035f; }
    }
    return s * 0.30f;
}

void game_audio_mix(int16_t *out, int frames) {
    while (q_r != q_w) {
        Req r = { queue[q_r % QSIZE].id, queue[q_r % QSIZE].pan, queue[q_r % QSIZE].vol };
        q_r = q_r + 1;
        if (r.id >= 0 && r.id < SFX_COUNT) handle_req(r.id, r.pan, r.vol);
    }
    if (music_req >= 0) { music_track = music_req; music_req = -1; m_step = 15; m_pos = 1e9f; }
    int track = music_track;
    for (int i = 0; i < frames; i++) {
        float l = 0, r = 0;
        for (int k = 0; k < NVOICE; k++) {
            Voice *v = &V[k];
            if (!v->on) continue;
            if (v->delay > 0) { v->delay -= 1.0f / RATE; continue; }
            float s = voice_sample(v);
            v->t += 1.0f / RATE;
            if (v->t >= v->dur) v->on = 0;
            float pl = 0.5f - v->pan * 0.4f, pr = 0.5f + v->pan * 0.4f;
            l += s * pl * 2; r += s * pr * 2;
        }
        float m = music_sample(track);
        l = (l * 0.55f + m); r = (r * 0.55f + m);
        l = l / (1 + fabsf(l)); r = r / (1 + fabsf(r));
        out[i * 2] = (int16_t)(l * 30000);
        out[i * 2 + 1] = (int16_t)(r * 30000);
    }
}
