# PC build (for testing). Needs SDL2: apt install libsdl2-dev / brew install sdl2
CC     ?= cc
CFLAGS ?= -O2 -Wall -std=gnu99

SRC = src/game.c src/art.c src/gfx.c src/audio.c

breachline: $(SRC) src/game.h src/internal.h src/platform_sdl.c
	$(CC) $(CFLAGS) $(SRC) src/platform_sdl.c $(shell sdl2-config --cflags --libs) -lm -o $@

# headless screenshot tool used for testing
tools/headless: $(SRC) src/game.h src/internal.h src/sprites_gen.h tools/headless.c
	$(CC) $(CFLAGS) -Isrc $(SRC) tools/headless.c -lm -o $@

psp:
	$(MAKE) -f Makefile.psp

clean:
	rm -f breachline tools/headless src/*.o *.elf *.prx PARAM.SFO EBOOT.PBP

.PHONY: psp clean
