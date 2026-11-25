/* BREACHLINE - PSP platform layer (PSPSDK only, no extra libraries).
 * Reads the pad, runs the game at 60 Hz, copies the frame into VRAM. */
#include <pspkernel.h>
#include <pspsdk.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspge.h>
#include <psppower.h>
#include <pspaudiolib.h>
#include <stddef.h>
#include <stdint.h>
#include "game.h"

PSP_MODULE_INFO("BREACHLINE", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

#define VRAM_STRIDE 512

static volatile int running = 1;

static int exit_callback(int a, int b, void *c) { (void)a; (void)b; (void)c; running = 0; return 0; }

static int callback_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cb = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    int th = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (th >= 0) sceKernelStartThread(th, 0, 0);
}

static uint32_t frame[SCREEN_W * SCREEN_H];

/* pspaudiolib calls this from its own thread; buf is interleaved stereo s16 */
static void audio_callback(void *buf, unsigned int length, void *userdata) {
    (void)userdata;
    game_audio_mix((int16_t *)buf, (int)length);
}

static uint32_t *vbuf[2];
static int back = 1;

static void present(void) {
    /* game is 0xAARRGGBB, PSP 8888 is 0xAABBGGRR: swap red and blue */
    uint32_t *dst = vbuf[back];
    const uint32_t *src = frame;
#define SWAP_RB(p) (((p) & 0xFF00FF00u) | (((p) >> 16) & 0xFFu) | (((p) & 0xFFu) << 16))
    for (int y = 0; y < SCREEN_H; y++) {
        uint32_t *d = dst + y * VRAM_STRIDE;
        for (int x = 0; x < SCREEN_W; x += 4) {
            uint32_t a = src[0], b = src[1], c = src[2], e = src[3];
            d[x] = SWAP_RB(a); d[x + 1] = SWAP_RB(b); d[x + 2] = SWAP_RB(c); d[x + 3] = SWAP_RB(e);
            src += 4;
        }
    }
    sceDisplayWaitVblankStart();
    sceDisplaySetFrameBuf(dst, VRAM_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
    back ^= 1;
}

int main(void) {
    pspSdkDisableFPUExceptions();
    setup_callbacks();
    scePowerSetClockFrequency(333, 333, 166);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    /* two framebuffers in VRAM, uncached mirror */
    uint32_t *vram = (uint32_t *)(0x40000000u | (uintptr_t)sceGeEdramGetAddr());
    vbuf[0] = vram; vbuf[1] = vram + VRAM_STRIDE * SCREEN_H;
    sceDisplaySetMode(0, SCREEN_W, SCREEN_H);
    sceDisplaySetFrameBuf(vbuf[0], VRAM_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);

    /* hold SELECT while the game starts = no sound (safe mode) */
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(&pad, 1);
    int sound = !(pad.Buttons & PSP_CTRL_SELECT);

    game_render_loading(frame);
    present();

    game_init();

    int audio_started = 0;
    uint64_t last = sceKernelGetSystemTimeWide();
    int64_t acc = 16667;
    while (running && !game_wants_quit()) {
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned b = 0;
        if (pad.Buttons & PSP_CTRL_LEFT)     b |= BTN_LEFT;
        if (pad.Buttons & PSP_CTRL_RIGHT)    b |= BTN_RIGHT;
        if (pad.Buttons & PSP_CTRL_UP)       b |= BTN_UP;
        if (pad.Buttons & PSP_CTRL_DOWN)     b |= BTN_DOWN;
        if (pad.Buttons & PSP_CTRL_CROSS)    b |= BTN_SHOOT;
        if (pad.Buttons & PSP_CTRL_SQUARE)   b |= BTN_KICK;
        if (pad.Buttons & PSP_CTRL_CIRCLE)   b |= BTN_ROLL;
        if (pad.Buttons & PSP_CTRL_TRIANGLE) b |= BTN_RELOAD;
        if (pad.Buttons & PSP_CTRL_START)    b |= BTN_START;
        if (pad.Buttons & PSP_CTRL_SELECT)   b |= BTN_SELECT;
        if (pad.Lx < 64)  b |= BTN_LEFT;
        if (pad.Lx > 192) b |= BTN_RIGHT;

        /* fixed 60 Hz game clock: if drawing was slow, run extra updates so the
           game keeps real-time speed (capped so it can't spiral) */
        uint64_t now = sceKernelGetSystemTimeWide();
        acc += (int64_t)(now - last);
        last = now;
        int steps = 0;
        while (acc >= 16667 && steps < 4) { game_update(b); acc -= 16667; steps++; }
        if (steps == 4) acc = 0;
        game_render(frame);
        present();

        if (sound && !audio_started) {          /* start sound once the game is on screen */
            pspAudioInit();
            pspAudioSetChannelCallback(0, audio_callback, NULL);
            audio_started = 1;
        }
    }

    if (audio_started) pspAudioEnd();
    sceKernelExitGame();
    return 0;
}
