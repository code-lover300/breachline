/* BREACHLINE - PSP platform layer (PSPSDK only, no extra libraries).
 * Reads the pad, runs the game at 60 Hz, copies the frame into VRAM. */
#include <pspkernel.h>
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

int main(void) {
    setup_callbacks();
    scePowerSetClockFrequency(333, 333, 166);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    /* two framebuffers in VRAM, uncached mirror */
    uint32_t *vram = (uint32_t *)(0x40000000u | (uintptr_t)sceGeEdramGetAddr());
    uint32_t *buf[2] = { vram, vram + VRAM_STRIDE * SCREEN_H };
    int back = 1;

    sceDisplaySetMode(0, SCREEN_W, SCREEN_H);
    sceDisplaySetFrameBuf(buf[0], VRAM_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);

    game_init();
    pspAudioInit();
    pspAudioSetChannelCallback(0, audio_callback, NULL);

    while (running && !game_wants_quit()) {
        SceCtrlData pad;
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
        /* analog stick also moves */
        if (pad.Lx < 64)  b |= BTN_LEFT;
        if (pad.Lx > 192) b |= BTN_RIGHT;

        game_update(b);
        game_render(frame);

        /* game is 0xAARRGGBB, PSP 8888 is 0xAABBGGRR: swap red and blue */
        uint32_t *dst = buf[back];
        const uint32_t *src = frame;
        for (int y = 0; y < SCREEN_H; y++) {
            uint32_t *d = dst + y * VRAM_STRIDE;
            for (int x = 0; x < SCREEN_W; x++) {
                uint32_t p = *src++;
                d[x] = (p & 0xFF00FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
            }
        }

        sceDisplayWaitVblankStart();
        sceDisplaySetFrameBuf(dst, VRAM_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_IMMEDIATE);
        back ^= 1;
    }

    pspAudioEnd();
    sceKernelExitGame();
    return 0;
}
