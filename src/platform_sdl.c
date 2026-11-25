/* BREACHLINE - PC platform layer (SDL2). Handy for testing before copying to a PSP.
 * Keys: arrows/WASD move, J or Z shoot, K or X kick, L or C roll, R reload,
 * Enter start, Backspace select, Esc quit. Gamepads work too. */
#include <SDL.h>
#include <stdint.h>
#include "game.h"

static uint32_t frame[SCREEN_W * SCREEN_H];

static void audio_cb(void *ud, Uint8 *stream, int len) {
    (void)ud;
    game_audio_mix((int16_t *)stream, len / 4);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_Window *win = SDL_CreateWindow("BREACHLINE", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       SCREEN_W * 2, SCREEN_H * 2, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                         SCREEN_W, SCREEN_H);
    SDL_GameController *pad = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) { pad = SDL_GameControllerOpen(i); break; }

    game_init();
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = AUDIO_RATE; want.format = AUDIO_S16SYS; want.channels = 2; want.samples = 1024; want.callback = audio_cb;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev) SDL_PauseAudioDevice(dev, 0);
    Uint64 freq = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter();
    double acc = 0;
    int running = 1;
    while (running && !game_wants_quit()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = 0;
            if (ev.type == SDL_CONTROLLERDEVICEADDED && !pad) pad = SDL_GameControllerOpen(ev.cdevice.which);
        }
        const Uint8 *k = SDL_GetKeyboardState(NULL);
        unsigned b = 0;
        if (k[SDL_SCANCODE_LEFT]  || k[SDL_SCANCODE_A]) b |= BTN_LEFT;
        if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) b |= BTN_RIGHT;
        if (k[SDL_SCANCODE_UP]    || k[SDL_SCANCODE_W]) b |= BTN_UP;
        if (k[SDL_SCANCODE_DOWN]  || k[SDL_SCANCODE_S]) b |= BTN_DOWN;
        if (k[SDL_SCANCODE_J] || k[SDL_SCANCODE_Z]) b |= BTN_SHOOT;
        if (k[SDL_SCANCODE_K] || k[SDL_SCANCODE_X]) b |= BTN_KICK;
        if (k[SDL_SCANCODE_L] || k[SDL_SCANCODE_C]) b |= BTN_ROLL;
        if (k[SDL_SCANCODE_R]) b |= BTN_RELOAD;
        if (k[SDL_SCANCODE_RETURN]) b |= BTN_START;
        if (k[SDL_SCANCODE_BACKSPACE]) b |= BTN_SELECT;
        if (pad) {
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  b |= BTN_LEFT;
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) b |= BTN_RIGHT;
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))    b |= BTN_UP;
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  b |= BTN_DOWN;
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A)) b |= BTN_SHOOT;
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X)) b |= BTN_KICK;
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B)) b |= BTN_ROLL;
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y)) b |= BTN_RELOAD;
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)) b |= BTN_START;
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK))  b |= BTN_SELECT;
            int lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
            if (lx < -16000) b |= BTN_LEFT;
            if (lx >  16000) b |= BTN_RIGHT;
        }

        /* fixed 60 Hz simulation, same as the PSP */
        Uint64 now = SDL_GetPerformanceCounter();
        acc += (double)(now - last) / (double)freq;
        last = now;
        if (acc > 0.25) acc = 0.25;
        while (acc >= 1.0 / 60.0) { game_update(b); acc -= 1.0 / 60.0; }

        game_render(frame);
        SDL_UpdateTexture(tex, NULL, frame, SCREEN_W * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
    SDL_Quit();
    return 0;
}
