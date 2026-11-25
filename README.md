# BREACHLINE

A side-view breach-and-clear shooter for PSP homebrew (and PC for testing).
You play a rescue operator: kick doors, clear rooms, and get the hostages out.
There are 5 operations: a house, a corner store, an office block, a night club and a tower.
Every building is a lit, furnished cutaway with its own theme, and the street, skyline, characters,
sound effects and music are all generated in code. There are no image or audio files.

## Controls (PSP)

| Button | Action |
|---|---|
| D-pad / analog | Move |
| Up / Down on stairs | Change floor |
| Cross | Shoot |
| Square | Kick door / secure hostage / shove a guard |
| Circle | Combat roll (dodges bullets) |
| Triangle | Reload |
| Start | Pause (Select restarts from pause) |

PC keys: arrows/WASD move, J or Z shoot, K or X kick, L or C roll, R reload, Enter start, Backspace select.

## How it plays

- Rooms behind closed doors are dark. You can't see in, and the guards can't see out.
- Kicking a door stuns anyone standing just behind it. That's your opening.
- Guards take a moment to react (watch for the red **!**). Heavies (olive, red visor) take 3 hits.
- Gunfire alerts the whole room.
- Your bullets keep flying after a kill. If you hit a hostage, the mission fails.
- An operation ends when every guard is down and every hostage is secured. You get rated on time, accuracy and hits taken.

## Building the PSP version (EBOOT.PBP)

**Easiest (any PC with Docker):**
```
docker run --rm -v "$PWD":/src -w /src pspdev/pspdev:latest make -f Makefile.psp
```

**With the pspdev toolchain installed** (https://pspdev.github.io):
```
make -f Makefile.psp
```

**With no setup:** push this folder to a GitHub repo. The included workflow
(`.github/workflows/build-psp.yml`) builds the EBOOT. You download it from the Actions tab under
*breachline-psp*.

## Running it

- **Real PSP** (needs custom firmware such as ARK-4 or PRO-C): copy `EBOOT.PBP` to
  `PSP/GAME/BREACHLINE/EBOOT.PBP` on the memory stick. It shows up under Game > Memory Stick.
- **Emulator:** open `EBOOT.PBP` in PPSSPP.

## Building the PC version

```
make            # needs SDL2 (apt install libsdl2-dev / brew install sdl2)
./breachline
```

## Making your own levels

Levels live at the top of `src/game.c` in the `LEVELS` table. Each operation has three floors
(top to bottom), and each character is a 16px tile:

```
#  wall          .  room           D  door
S  stairs (must line up with an S on the floor above or below)
P  player start  E  guard          W  patrolling guard
B  heavy guard   H  hostage
```

Rows can be different lengths (max 64); short ones get padded with wall. A floor that's just `"#"`
is drawn as the outside of the building. Keep doors at least one tile away from stairs. Run `make tools/headless && ./tools/headless /tmp 0 "1:-"`
to check your levels: it reports stairs that lead nowhere, missing walls, and player-start problems.

## Code layout

```
src/game.c          gameplay, AI, camera, effects, HUD, menus, scoring, save data
src/art.c           baked lit buildings, furniture, exterior, sky, sprite drawing, doors
src/sprites_gen.h   character pixel art (generated - edit tools/sprites.py instead)
tools/sprites.py    the character sprites, drawn as text grids; run it to preview and re-export
src/gfx.c           drawing primitives and the bitmap font
src/audio.c         procedural sound effects and music
src/game.h          the small API the platform layers call
src/platform_psp.c  PSP: pad input, VRAM double buffer, audio, 333 MHz clock
src/platform_sdl.c  PC: SDL2 window, audio, keyboard and gamepad
tools/headless.c    scripted-input / autopilot screenshot tool for testing
```

Progress (unlocked operations, best stars and scores) is saved to `breachline.sav` next to the game.

Test every level with the autopilot: `./tools/headless /tmp 2 "bot9000"` prints whether it cleared.

## Editing the characters

The operator, guards, heavies and hostages are pixel art drawn as text grids in `tools/sprites.py`
(each letter is a palette colour, `.` is empty). After editing:

```
python3 tools/sprites.py /tmp/preview.png          # see every frame
python3 tools/sprites.py export src/sprites_gen.h  # update the game
```
