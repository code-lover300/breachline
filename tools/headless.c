/* Headless runner: plays a scripted input sequence and writes PPM screenshots.
 * usage: headless <outdir> <level|-1 for title> <script>
 * script: comma-separated "frames:buttons" where buttons are letters
 *   L R U D (dirs) X shoot Q kick O roll T reload S start E select, '-' none,
 *   or "shot" to save a screenshot. Example: "30:-,shot,40:R,10:Q,shot" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
unsigned game_debug_bot(void);
int game_debug_info(int *hp, int *left, int *score);

static uint32_t fb[SCREEN_W * SCREEN_H];
static int shot_n;

static void save(const char *dir) {
    char path[512];
    snprintf(path, sizeof path, "%s/shot_%02d.ppm", dir, shot_n++);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        unsigned char px[3] = { (fb[i] >> 16) & 255, (fb[i] >> 8) & 255, fb[i] & 255 };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    printf("saved %s\n", path);
}

static unsigned parse_buttons(const char *s) {
    unsigned b = 0;
    for (; *s; s++) switch (*s) {
        case 'L': b |= BTN_LEFT; break;   case 'R': b |= BTN_RIGHT; break;
        case 'U': b |= BTN_UP; break;     case 'D': b |= BTN_DOWN; break;
        case 'X': b |= BTN_SHOOT; break;  case 'Q': b |= BTN_KICK; break;
        case 'O': b |= BTN_ROLL; break;   case 'T': b |= BTN_RELOAD; break;
        case 'S': b |= BTN_START; break;  case 'E': b |= BTN_SELECT; break;
    }
    return b;
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s outdir level script\n", argv[0]); return 2; }
    int problems = game_debug_validate();
    printf("level validation problems: %d\n", problems);
    game_init();
    int lv = atoi(argv[2]);
    if (lv >= 0) game_debug_start_level(lv);

    char *script = strdup(argv[3]);
    for (char *tok = strtok(script, ","); tok; tok = strtok(NULL, ",")) {
        if (!strcmp(tok, "shot")) { game_render(fb); save(argv[1]); continue; }
        if (!strncmp(tok, "bot", 3)) {          /* botN: autopilot for N frames, screenshot every 600 */
            int n = atoi(tok + 3);
            for (int i = 0; i < n; i++) {
                game_update(game_debug_bot());
                if (i % 600 == 599) { game_render(fb); save(argv[1]); }
                int hp, left, sc, st = game_debug_info(&hp, &left, &sc);
                if (st == 6 || st == 7 || st == 8) {   /* clear / fail / win */
                    for (int k = 0; k < 120; k++) game_update(0);
                    printf("RESULT state=%s frame=%d hp=%d left=%d score=%d\n",
                           st == 7 ? "FAILED" : "CLEARED", i, hp, left, sc);
                    break;
                }
            }
            continue;
        }
        int frames = atoi(tok);
        char *c = strchr(tok, ':');
        unsigned b = c ? parse_buttons(c + 1) : 0;
        for (int i = 0; i < frames; i++) {
            /* press-type buttons only count on the first frame of a segment */
            game_update(i == 0 ? b : (b & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN | BTN_SHOOT)));
        }
    }
    game_render(fb);
    save(argv[1]);
    return problems ? 1 : 0;
}
