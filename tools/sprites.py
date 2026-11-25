"""BREACHLINE character sprites: original hand-placed pixel art.
Parts are drawn facing right; frames are composed from parts, previewed to PNG,
and exported to src/sprites_gen.h for the game. Run: python3 tools/sprites.py"""
import sys
from PIL import Image

def grid(rows):
    w = max(len(r) for r in rows)
    return [r.ljust(w, '.') for r in rows]

# ---------------- operator parts (v2, larger) ----------------
OP_HEAD = grid([
".....oooooo",
"...ooJJJJHHoo",
"..oJJJHHHHHHHo",
".oJJHHHHHHHHHxo",
".oJHHHHHHHHHHxxo",
"oJHHHHHHHHHHHxxo",
"oHHHHHHhhhhhhhhho",
"oHHhhoooooooooooo",
"ohhoxxxxxxxoGGGGGo",
"ohoMMMMMMMoGGGGGgo",
".ooMMMMMMMMooooooo",
".ooMMMMMMMMMMMMo",
"..omMMMMMMMMMMo",
"...ommmmmmmmo",
"....oooooooo",
])
OP_TORSO = grid([
".....oooooooooo",
"...ooVVUUUUUUUUo",
"..oVVUUUUUUUUUUUo",
".oAAUUCCCCCCCCCCo",
".oAaUCKKKKKKKKCCCo",
"oVUuCKCCCCCCCCCcCo",
"oVUuCCoooooooCCcCo",
"oVUuCoKKoKKoCCCcCo",
"oVUuCoCCoCCoCCCcCo",
"oVUuCooooooooCCcCo",
"oUUuCCCCCCCCCCCcCo",
"ouuucccccccccccco",
"oLxxYYYYxxYYYYxxo",
"oxxxxxxxxxxxxxxxo",
".ooooooooooooooo",
])
OP_ARM_AIM = grid([
"...........................oooo",
"....................oooooooxYYo",
"..ooo..............oxxxxxxxxxxo",
".oVUUo.........ooooooYYYYYYYYYYYYYYYoooo",
"oVUUUUoo......oYXXXXXXXXXXXXXXXXXXXXXXXXXo",
"oUUUUUUUoooooooXXXXXXXXXXXXXXXXXXXXXXXXXXo",
"ouUUUUUUUUUULLLXXooooooooXXXoooooooooooo",
".ouuuuuuuuLLLLLoXXo......oLLLo",
"..ooooooooLLLLoxXXo......oLLo",
"..........oooo.oxXo.......oo",
"................oo",
])
OP_ARM_RECOIL = [r[2:] if len(r) > 2 else r for r in OP_ARM_AIM]
OP_ARM_LOW = grid([
"..ooo",
".oVUUo",
"oVUUUUo",
"oUUUUUUo",
"ouUUUUUUoooooooooo",
".ouuuuuLLLXXXXXXXXXooooo",
"..ooooLLLLYYYYYYYYYYYYYYYYo",
"......oLLoXXXXXXXXXXXXXXXXXo",
".......oooxXXoooooooXXooooo",
"..........oxXo.....oLLo",
"...........oo.......oo",
])
LEG_STAND = grid([
"..ooooooooooo",
".oQPPPPPPPPPPo",
".oQPPPPPPPPPPPo",
".oQPPPPooPPPPPo",
".oQPPPo..oPPPPo",
".oQPPPo..opPPPo",
"okkkkko..okkkkko",
"okKkkko..okKkkko",
".oPPPo....oPPPPo",
".oPPPo....oPPPPo",
".oPPpo....oPPPpo",
".oPPpo....oPPPpo",
"oBBBBBo..oBBBBBBo",
"oBbbbbbo.oBbbbbbbo",
".oooooo...ooooooo",
])
LEG_WALK = [
grid([
"...ooooooooooo",
"..oQPPPPPPPPPPo",
"..oQPPPPPPPPPPPo",
".oQPPPPooooPPPPPo",
".oQPPPo....oPPPPPo",
"oQPPPo......oPPPPo",
"okkkko.......okkkko",
"okKkko.......okKkkoo",
"oPPPo.........oPPPPo",
"oPPpo..........oPPPo",
"oPPpo..........oPPPo",
"oPPpo...........oPPPo",
"oBBBBo.........oBBBBBo",
"obbbbbo........oBbbbbbo",
".ooooo..........ooooooo",
]),
grid([
"..ooooooooooo",
".oQPPPPPPPPPPo",
".oQPPPPPPPPPPo",
"..oQPPPPPPPPo",
"..oQPPPPPPPPo",
"...oPPPPPPPo",
"...okkkkkkko",
"...okKkkkkko",
"....oPPPPPo",
"....oPPPPPo",
"....oPPpPPo",
"....oPPopPo",
"...oBBBBBBBo",
"...oBbbbbbbbo",
"....ooooooooo",
]),
None, None]
LEG_WALK[2] = grid([
"...ooooooooooo",
"..oQPPPPPPPPPPo",
"..oQPPPPPPPPPPPo",
".oQPPPPPooooPPPPo",
".oQPPPo.....oPPPPo",
"oQPPPo.......oPPPo",
"okkkko........okkko",
"okKkko........okKko",
"oPPPo..........oPPPo",
"oPPpo..........oPPPo",
"oPPpo...........oPPo",
"oPPpo...........oPPo",
"oBBBBo.........oBBBBBo",
"obbbbbo........oBbbbbbo",
".ooooo..........ooooooo",
])
LEG_WALK[3] = LEG_WALK[1]
LEG_KICK = grid([
"..ooooooooooooooo",
".oQPPPPPPPPPPPPPPPoooooo",
".oQPPPPPPPPPPPPPPPkkkkkPPPPooo",
".oQPPPPooooooooPPPkKkkkPPPPPBBBo",
".oQPPPo.......ooookkkkkoooooBBBbo",
".oQPPPo...........ooooo....oBBbbo",
"okkkkko.....................oooo",
"okKkkko",
".oPPPo",
".oPPPo",
".oPPpo",
".oPPpo",
"oBBBBBo",
"oBbbbbbo",
".oooooo",
])

# ---------------- guard parts ----------------
GD_HEAD = grid([
".....oooooo",
"...ooHHHHHHoo",
"..oHHJJHHHHHHo",
".oHJJHHHHHHHHHo",
".oHJHHHHHHhhHHHo",
"oHHHHHHHHhoooHHo",
"oHHHHHHHhoSSSSoHo",
"oHHHHHHhoSSSSSSoo",
"oHHHHHhoSSSoESSSo",
"oHHHHhoRRRRRRRRRo",
".oHHhoRRRRRRRRRRo",
".oHhoRRrRRRRRRRo",
"..ooorrrRRRRRro",
"....ooorrrrroo",
"........oooo",
])
GD_TORSO = grid([
"....ooooooooo",
"..ooHHHUUUUUUoo",
".oHhhUUUUUUUUUUo",
"oVUUUUUUUWWUUUUo",
"oVUUUUUUUWwUUUUUo",
"oVUUUUUUUWwUUUUUo",
"oVUUuUUUUWwUUUUUo",
"oVUUuUUUUWwUUUUUo",
"oVUUuUoooooooUUUo",
"oVUUuUoUUUUUoUUUo",
"ouUUuUoooooooUUo",
"ouuuuuuuuuuuuuuo",
"oxxxxxxxxxxxxxxo",
".oooooooooooooo",
])
GD_ARM_AIM = grid([
"..............................",
"......................oo",
"..ooo...............ooTTo",
".oVUUo.............oTTTToooooooooooooooo",
"oVUUUUoo..........oXXXXXXXXXXXXXXXXXXXXXXo",
"oUUUUUUUooooooooSSSXXXXXXXXXXXXXXXXXXXXXo",
"ouUUUUUUUUUUUUSSSSoXoTTTTTTTTTooooooooo",
".ouuuuuuuuuuuSSSo.oXXo.oSSoooo",
"..oooooooooooooo...oXXo.oo",
"....................oXo",
".....................o",
])
GD_ARM_RECOIL = [r[2:] if len(r) > 2 else r for r in GD_ARM_AIM]
GD_ARM_LOW = grid([
"..ooo",
".oVUUo",
"oVUUUUo",
"oUUUUUUo",
"ouUUUUUUoooooo",
".ouuuuuuSSSTTTTTXXXXooooo",
"..oooooSSSSTTTTTXXXXXXXXXXXo",
".......oSSoXXXXooTTTTTToooooo",
"........oooXXXXo.oSSo",
"...........oXXo...oo",
"............oo",
])
GD_LEG_STAND = grid([
"..ooooooooooo",
".oQPPPPPPPPPPo",
".oQPPPPPPPPPPPo",
".oQPPPPooPPPPPo",
".oQPPPo..oPPPPo",
".oQPPPo..opPPPo",
".oQPPPo..oPPPPo",
".oQPPpo..oPPPpo",
".oPPPpo...oPPPPo",
".oPPPpo...oPPPPo",
".oPPpo....oPPPpo",
".oPPpo....oPPPpo",
"oWWWWWo..oWWWWWWo",
"oWbbbbbo.oWbbbbbbo",
".oooooo...ooooooo",
])


# ---------------- guns (drawn separately, then gripped by generated arms) ----------------
GUN_OP = dict(rows=grid([
"...........oooooooo",
"..........oZZZZZZZNo",
"..........oxZxxxxxxo...........ooo",
"....oooo..ooooooooooooooooooooooYYo",
"...oYYYYooYYYYYYYYYYYYYYYYYYYYYYYYYoooooooo",
"..oYXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXZZZZZZZo",
"ooXXXXXXXXXXxXxXxXxXxXxXxXxXxXXXXXXXxxxxxxxo",
"oXXXXoXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXxxxxxxxo",
"oXXXooxXXXXoooxXXXXXoooooxXXXooooooooooooooo",
"oXXXo.oxXXo..oxXXXo....oxXXo",
"oxxxo..oxXo..oxXXo.....oxXo",
"ooooo...oo...oxXXo......oo",
".............oxXo",
"..............oo",
]), grip=(8, 9), support=(25, 9), butt=(2, 6), lens='N')

GUN_GD = dict(rows=grid([
"......................ooooooooooooo",
"ooooo.........ooooooooxxxxxxxxxxxxxo",
"oTTTToo......oZZZZZZZTTTTTTTTTxxxxxxxooooooo",
"oTTTTTTooooooXXXXXXXXTTTTTTTTTXXXXXXXXXXXXXo",
"otTTTTTTTTTTXXXXXXXXXtttttttttxooooooooooooo",
"ottTTTTTTtooXXXooXXXXooooooooooo",
".oottTTTto.oxXXo.oXXXXo",
"....ooooo..oxXo..oxXXXo",
"............oo....oxXXXo",
"...................oxXXo",
"....................ooo",
]), grip=(13, 7), support=(25, 5), butt=(1, 3), lens=None)

GUN_HV = dict(rows=grid([
"................oooooo",
"...............oZZZZZZo",
"ooooo..........oxooooxo",
"oXXXXooooooooooXXXXXXXXXXoooooooooooooooooo",
"oXXXXXXXXXXXXXXXXXXXXXXXXZoZoZoZoZoZoZoZXXXXoooo",
"oYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYXXXxxxo",
"oXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXxxxo",
"oXXxooooxXXXoZZZZZZZZZZooooooooooooooooooooooooo",
"ooooo...oxXXoZXXXXXXXXZo",
".........oxXoZXXXXXXXXZo",
"..........ooZZZZZZZZZZZo",
"...........oooooooooooo",
]), grip=(10, 8), support=(28, 7), butt=(1, 4), lens=None)

def put(c, x, y, ch):
    if 0 <= y < len(c) and 0 <= x < len(c[0]): c[y][x] = ch

def thick_line(c, x0, y0, x1, y1, ch, r):
    n = max(abs(x1 - x0), abs(y1 - y0), 1)
    for k in range(n + 1):
        x = round(x0 + (x1 - x0) * k / n); y = round(y0 + (y1 - y0) * k / n)
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if abs(dx) + abs(dy) <= r + (1 if r > 1 else 0): put(c, x + dx, y + dy, ch)

def sleeve(c, pts, kind):
    sl, sd = ('C', 'c') if kind == 'hv' else ('U', 'u')
    for a, b in zip(pts, pts[1:]): thick_line(c, a[0], a[1], b[0], b[1], 'o', 2)
    for a, b in zip(pts, pts[1:]): thick_line(c, a[0], a[1], b[0], b[1], sl, 1)
    for a, b in zip(pts, pts[1:]): thick_line(c, a[0], a[1] + 1, b[0], b[1] + 1, sd, 0)

def glove(c, x, y, ch):
    for dy in range(-2, 3):
        for dx in range(-2, 3):
            if abs(dx) + abs(dy) <= 3: put(c, x + dx, y + dy, 'o')
    for dy in range(-1, 2):
        for dx in range(-1, 2):
            if abs(dx) + abs(dy) <= 1 or (dx, dy) == (1, 1): put(c, x + dx, y + dy, ch)

def place_gun(c, gun, sx, sy, mode):
    """stamp the gun so its butt sits at (sx, sy); returns canvas positions of grip + support."""
    rows = gun['rows']; bx, by = gun['butt']
    ox, oy = sx - bx, sy - by
    if mode == 'recoil': ox -= 2
    def shear(i): return (i // 7) if mode == 'low' else 0
    for j, r in enumerate(rows):
        for i, ch in enumerate(r):
            if ch != '.': put(c, ox + i, oy + j + shear(i), ch)
    g = (ox + gun['grip'][0], oy + gun['grip'][1] + shear(gun['grip'][0]))
    s_ = (ox + gun['support'][0], oy + gun['support'][1] + shear(gun['support'][0]))
    return g, s_

def legs_recolor(rows, mapping):
    return [''.join(mapping.get(c, c) for c in r) for r in rows]

# ---------------- heavy parts ----------------
HV_HEAD = grid([
".....ooooooo",
"...ooJJJHHHHoo",
"..oJJHHHHHHHHHo",
".oJHHHHHHHHHHHHo",
".oHHHHHHHHHHHHHHo",
"oJHHHHHHHHHHHHHHo",
"oHHHHHhhhhhhhhhhho",
"oHHHhooooooooooooo",
"oHHhoxxxxGGGGGGGxo",
"oHHhoxxxxxxxxxxxxo",
"oHHhoxxxxoxoxoxxo",
".oHhoxxxxxxxxxxxo",
"..ohhoxxxxxxxxxo",
"...ooooooooooooo",
])
HV_TORSO = grid([
"...ooooo..oooooo",
"..oJJJJJooKKKKKCo",
".oJCCCCCCooKCCCCCo",
"oJCCCCCCCCoCCCCCCCo",
"oCCCCCCCCoKKKKKKKCCo",
"oCcCCCCCCoCCCCCCCcCo",
"oCcCCCCCCoKKKKKKKcCo",
"oCcCCCCCCoCCCCCCCcCo",
"oCcCCCCCCoKKKKKKKcCo",
"oCcCCCCCCoCCCCCCCcCo",
"oCccccccooccccccccCo",
"oxxxxxxxxxxxxxxxxxxo",
"oLxxYYYYxxxxYYYYxxxo",
".ooooooooooooooooooo",
])
HV_ARM_AIM = grid([
"..............................",
"..oooo.........oooooooooooooooooooo",
".oJCCCo.......oXXXXXXXXXXXXXXXXXXXXXooo",
"oJCCCCCoo....oYYYYYYYYYYYYYYYYYYYYYYYYYYo",
"oCCCCCCCCoooLLXXXXXXXXXXXXXXXXXXXXXXXXXXXo",
"ocCCCCCCCCLLLLXXooooxxxxxxxooooooXXXXXooo",
".occcccccLLLLLoxXXXo.oxxxxxo....oLLXXXo",
"..ooooooooooooxXXXXXo.ooooo.....oLLLoo",
"..............oxXXXXXo...........ooo",
"...............oxXXXo",
"................ooo",
])
HV_ARM_RECOIL = [r[2:] if len(r) > 2 else r for r in HV_ARM_AIM]
HV_LEG_STAND = grid([
"..oooooooooooooo",
".oQPPPPPPPPPPPPPo",
".oQPPPPPPPPPPPPPPo",
".oQPPPPPooPPPPPPPo",
"oCCCCCCo..oCCCCCCo",
"oCKKKKCo..oCKKKKCo",
"oCCCCCCo..oCCCCCCo",
"oCccccCo..oCccccCo",
".oPPPPo....oPPPPPo",
".oPPPpo....oPPPPpo",
".oPPPpo....oPPPPpo",
".oPPPpo....oPPPPpo",
"oBBBBBBo..oBBBBBBBo",
"oBbbbbbbo.oBbbbbbbbo",
".ooooooo...oooooooo",
])

# ---------------- hostage ----------------
HS_KNEEL = grid([
"...oo.....oo",
"..oSSo...oSSo",
"..oSSoooooSSo",
"...oSHHHHHSo",
"..oHHHHHHHHHo",
"..oHHHHHHHHHHo",
"..oHHSSSSSSSo",
"..oHSSSSESSESo",
"..oHSSSSSSSSo",
"...oSSSSsssSo",
"....oSSSSSSo",
"...ooFFoFFoo",
"..oYYFFFFFYYo",
".oYYYYoFoYYYYo",
".oYYyYYYYYYYYo",
".oYYyYYYYYYYYo",
".oYYyYYYYYYYYo",
".oyyyyyyyyyyyo",
"oPPPPPPPPPPPPPPo",
"oPPPPPPPPPPPPPPPo",
"oPPpppppppPPPPPPo",
"oBBBBo.....oPPPPo",
".oooo.......oooo",
])
HS_STAND = grid([
"....ooooooo",
"...oHHHHHHHo",
"..oHHHHHHHHHo",
"..oHHHHHHHHHHo",
"..oHHSSSSSSSo",
"..oHSSSSESSESo",
"..oHSSSSSSSSo",
"...oSSSSsssSo",
"....oSSSSSSo",
"...ooFFoFFoo",
"..oYYFFFFFYYo",
".oYYYYoFoYYYYo",
".oYYyYYYYYYYYo",
".oYoyYYYYYYoYo",
".oYoyYYYYYYoYo",
".oSoyYYYYYYoSo",
".oSoyyyyyyyoSo",
"..ooPPPPPPPoo",
"...oPPPPPPPo",
"...oPPPooPPo",
"...oPPPooPPo",
"...oPPPooPPo",
"...oPPPooPPo",
"...oPPpooPPo",
"..oBBBBooBBBo",
"..oooooooooooo",
])

# ---------------- palettes ----------------
OUT = (16, 18, 24)
PAL_OP = {
 'o': OUT, 'H': (70, 78, 70), 'h': (46, 52, 48), 'J': (112, 122, 106),
 'G': (255, 150, 50), 'g': (220, 90, 30), 'x': (30, 32, 36),
 'M': (84, 88, 98), 'm': (58, 62, 70), 'S': (222, 168, 124),
 'U': (80, 92, 112), 'u': (56, 66, 82), 'V': (114, 128, 150),
 'C': (164, 138, 96), 'c': (124, 102, 70), 'K': (196, 172, 128),
 'A': (60, 220, 206), 'a': (30, 150, 140),
 'P': (70, 80, 96), 'p': (50, 58, 72), 'Q': (98, 110, 128), 'k': (36, 38, 44), 'K': (196, 172, 128),
 'B': (36, 34, 32), 'b': (22, 20, 20), 'L': (40, 40, 44),
 'X': (46, 50, 58), 'Y': (104, 112, 124), 'W': (240, 240, 230), 'Z': (140, 148, 160), 'N': (90, 230, 255),
}
PAL_GD = dict(PAL_OP)
PAL_GD.update({
 'E': (20, 20, 24),
 'H': (86, 90, 104), 'h': (60, 63, 74), 'J': (120, 124, 140),
 'U': (92, 96, 110), 'u': (66, 69, 80), 'V': (120, 124, 140), 'W': (210, 206, 196), 'w': (170, 166, 156),
 'R': (204, 48, 58), 'r': (150, 30, 40), 'S': (200, 146, 110),
 'P': (54, 70, 104), 'p': (38, 50, 78), 'Q': (80, 98, 136),
 'b': (24, 24, 28), 'T': (150, 92, 48), 't': (104, 60, 30), 'X': (52, 54, 60), 'Z': (120, 124, 132),
})
PAL_HV = dict(PAL_OP)
PAL_HV.update({
 'H': (66, 80, 50), 'h': (44, 54, 34), 'J': (100, 118, 74), 'G': (255, 60, 50), 'x': (24, 26, 22),
 'C': (82, 96, 60), 'c': (56, 66, 42), 'K': (112, 128, 84),
 'P': (46, 52, 38), 'p': (32, 36, 26), 'Q': (66, 74, 54),
 'L': (30, 30, 30), 'X': (44, 46, 50), 'Y': (90, 96, 104), 'Z': (112, 118, 104), 'B': (26, 26, 24), 'b': (16, 16, 14),
})
PAL_HS = {
 'E': (24, 24, 28),
 'o': OUT, 'S': (230, 182, 140), 's': (180, 120, 96), 'H': (92, 56, 34), 'W': (30, 30, 30),
 'F': (246, 242, 230), 'Y': (242, 198, 70), 'y': (200, 156, 44),
 'P': (60, 76, 108), 'p': (44, 56, 82), 'B': (58, 44, 34),
}

# ---------------- composition ----------------
def blank(w, h): return [['.'] * w for _ in range(h)]
def stamp(c, part, x, y):
    for j, r in enumerate(part):
        for i, ch in enumerate(r):
            if ch != '.' and 0 <= y + j < len(c) and 0 <= x + i < len(c[0]):
                c[y + j][x + i] = ch

W_, H_ = 66, 50
FOOT_X = 10       # x of the character's centre in the canvas
def compose(head, torso, arm, legs, bob=0, head_dx=0, arm_dy=0, legs_dx=0, head_dy=0):
    c = blank(W_, H_)
    stamp(c, legs, 3 + legs_dx, H_ - len(legs))
    ty = H_ - len(legs) - len(torso) + 2 + bob
    stamp(c, torso, 4, ty)
    stamp(c, head, 5 + head_dx, ty - len(head) + 2 + head_dy)
    if isinstance(arm, tuple):            # (gun, mode, glove colour)
        gun, mode, gl = arm[:3]
        kind = arm[3] if len(arm) > 3 else None
        shx, shy = 9, ty + 4
        g, sp = place_gun(c, gun, 13, ty + 5 + (1 if mode == 'low' else 0), mode)
        sleeve(c, [(shx, shy), (shx + 2, g[1] + 1), g], kind)
        sleeve(c, [(shx + 3, shy + 1), ((shx + sp[0]) // 2 + 2, sp[1] + 2), sp], kind)
        glove(c, g[0], g[1], gl); glove(c, sp[0], sp[1], gl)
    elif arm: stamp(c, arm, 2, ty + 1 + arm_dy)
    return c

def rot_dead(c):
    # lie on the back: rotate 90 deg (head to the left), drop to the floor
    h, w = len(c), len(c[0])
    r = [[c[h - 1 - j][i] for j in range(h)] for i in range(w)]   # rotate clockwise
    r = [row[::-1] for row in r][::-1]                               # head to the left side
    r = [row for row in r if any(ch != '.' for ch in row)]
    out = blank(W_ + 8, H_)
    stamp(out, [''.join(x) for x in r], 0, H_ - len(r))
    return out

LEG_TUCK = grid([
"..ooooooooo",
".oQPPPPPPPPoooo",
".oQPPPPPPPPkkkko",
"..oPPPPPPPPkKkko",
"...ooooBBBBBBBo",
".......obbbbbbo",
"........ooooooo",
])
def roll_frames(head, torso, arm):
    c = blank(W_, H_)
    stamp(c, LEG_TUCK, 4, H_ - 9)
    stamp(c, torso, 2, H_ - 9 - len(torso) + 6)
    stamp(c, head, 6, H_ - 9 - len(torso) + 6 - len(head) + 4)
    rows = [''.join(r) for r in c]
    rows = [r for r in rows if r.strip('.')]
    x0 = min(len(r) - len(r.lstrip('.')) for r in rows); x1 = max(len(r.rstrip('.')) for r in rows)
    sq = [r[x0:x1] for r in rows]
    n = max(len(sq), len(sq[0]))
    sq = [r.ljust(n, '.') for r in sq] + ['.' * n] * (n - len(sq))
    out = []
    cur = [list(r) for r in sq]
    for k in range(4):
        f = blank(W_, H_)
        stamp(f, [''.join(r) for r in cur], 10 - n // 2 + 2, H_ - n)
        out.append(f)
        cur = [list(r) for r in zip(*cur[::-1])]   # rotate clockwise 90
    return out

def frames_for(kind):
    F = {}
    if kind in ('op', 'gd', 'hv'):
        head = {'op': OP_HEAD, 'gd': GD_HEAD, 'hv': HV_HEAD}[kind]
        torso = {'op': OP_TORSO, 'gd': GD_TORSO, 'hv': HV_TORSO}[kind]
        gun = {'op': GUN_OP, 'gd': GUN_GD, 'hv': GUN_HV}[kind]
        gl = {'op': 'L', 'gd': 'S', 'hv': 'L'}[kind]
        aim, rec, low = (gun, 'aim', gl, kind), (gun, 'recoil', gl, kind), (gun, 'low', gl, kind)
        stand = {'op': LEG_STAND, 'gd': GD_LEG_STAND, 'hv': HV_LEG_STAND}[kind]
        walk = LEG_WALK if kind == 'op' else [legs_recolor(w, {'k': 'P', 'B': 'W' if kind == 'gd' else 'B'}) for w in LEG_WALK]
        if kind == 'hv': walk = [legs_recolor(w, {'k': 'C'}) for w in LEG_WALK]
        F['idle0'] = compose(head, torso, aim, stand)
        F['idle1'] = compose(head, torso, aim, stand, bob=1)
        for i in range(4): F['walk%d' % i] = compose(head, torso, aim, walk[i], bob=1 if i in (1, 3) else 0)
        F['shoot'] = compose(head, torso, rec, stand, head_dx=-1)
        F['low0'] = compose(head, torso, low, stand)
        F['low1'] = compose(head, torso, low, stand, bob=1)
        for i in range(4): F['lwalk%d' % i] = compose(head, torso, low, walk[i], bob=1 if i in (1, 3) else 0)
        F['kick'] = compose(head, torso, low if kind != 'hv' else aim, LEG_KICK if kind == 'op' else legs_recolor(LEG_KICK, {'k': 'P'}), head_dx=-2, legs_dx=0)
        F['stun0'] = compose(head, torso, low, stand, head_dx=-1, bob=1)
        F['stun1'] = compose(head, torso, low, stand, head_dx=1, bob=1)
        F['dead'] = rot_dead(compose(head, torso, None, stand))
        if kind == 'op':
            for i, f in enumerate(roll_frames(head, torso, None)): F['roll%d' % i] = f
    else:
        c = blank(W_, H_); stamp(c, HS_KNEEL, 3, H_ - len(HS_KNEEL)); F['kneel0'] = c
        c = blank(W_, H_); stamp(c, HS_KNEEL, 4, H_ - len(HS_KNEEL)); F['kneel1'] = c
        c = blank(W_, H_); stamp(c, HS_STAND, 3, H_ - len(HS_STAND)); F['stand'] = c
        F['dead'] = rot_dead(c)
    return F

def render(F, pal, scale=6, bg=(58, 62, 72)):
    keys = list(F.keys())
    w = max(len(F[k][0]) for k in keys)
    img = Image.new('RGB', ((w + 2) * len(keys) * scale, (H_ + 2) * scale), bg)
    px = img.load()
    for n, k in enumerate(keys):
        for j, row in enumerate(F[k]):
            for i, ch in enumerate(row):
                if ch == '.': continue
                col = pal.get(ch, (255, 0, 255))
                for a in range(scale):
                    for b in range(scale):
                        px[((w + 2) * n + 1 + i) * scale + a, (1 + j) * scale + b] = col
    return img

FRAME_NAMES = ['idle0','idle1','walk0','walk1','walk2','walk3','shoot','low0','low1','lwalk0','lwalk1','lwalk2','lwalk3',
               'kick','stun0','stun1','dead','roll0','roll1','roll2','roll3','kneel0','kneel1','stand']
GUN = set('XYxZ')
def export(path):
    sets = [('op', PAL_OP), ('gd', PAL_GD), ('hv', PAL_HV), ('hs', PAL_HS)]
    out = ['/* generated by tools/sprites.py - original pixel art for BREACHLINE */',
           '#define SPR_NFRAMES %d' % len(FRAME_NAMES),
           'enum { ' + ', '.join('SF_' + n.upper() for n in FRAME_NAMES) + ' };',
           'typedef struct { int16_t x0, y0, w, h, mx, my; const uint8_t *px; } SprFrame;',
           '#define SPR_AX 11', '#define SPR_FOOT %d' % H_]
    pal_lines, frame_tables = [], []
    for kind, pal in sets:
        F = frames_for(kind)
        keys = sorted(set(ch for fr in F.values() for row in fr for ch in row if ch != '.'))
        idx = {ch: i + 1 for i, ch in enumerate(keys)}
        cols = ['0x00000000'] + ['0xFF%02X%02X%02X' % pal.get(ch, (255, 0, 255)) for ch in keys]
        pal_lines.append('static const uint32_t SPR_PAL_%s[%d] = { %s };' % (kind, len(cols), ', '.join(cols)))
        emis = set('GgAN') if kind != 'gd' else set()
        pal_lines.append('static const uint8_t SPR_EMI_%s[%d] = { %s };' % (kind, len(cols), ', '.join(['0'] + ['1' if ch in emis else '0' for ch in keys])))
        pal_lines.append('#define SPR_NPAL_%s %d' % (kind, len(cols)))
        entries = []
        for name in FRAME_NAMES:
            if name not in F: entries.append('{0,0,0,0,0,0,NULL}'); continue
            fr = F[name]
            ys = [j for j, r in enumerate(fr) if any(c != '.' for c in r)]
            xs = [i for r in fr for i, c in enumerate(r) if c != '.']
            x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
            mx, my = 0, 0
            for j, r in enumerate(fr):
                for i, c in enumerate(r):
                    if c in GUN and i > mx and j < 40: mx, my = i, j
            data = []
            for j in range(y0, y1 + 1):
                for i in range(x0, x1 + 1):
                    data.append(idx.get(fr[j][i], 0) if fr[j][i] != '.' else 0)
            nm = 'SPR_%s_%s' % (kind, name)
            out.append('static const uint8_t %s[] = {%s};' % (nm, ','.join(map(str, data))))
            entries.append('{%d,%d,%d,%d,%d,%d,%s}' % (x0, y0, x1 - x0 + 1, y1 - y0 + 1, mx, my, nm))
        frame_tables.append('static const SprFrame SPR_%s[SPR_NFRAMES] = { %s };' % (kind, ', '.join(entries)))
    open(path, 'w').write('\n'.join(out + pal_lines + frame_tables) + '\n')

if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[1] == 'export':
        export(sys.argv[2]); sys.exit(0)
    import os
    sets = [('op', PAL_OP), ('gd', PAL_GD), ('hv', PAL_HV), ('hs', PAL_HS)]
    imgs = [render(frames_for(k), p, 4) for k, p in sets]
    W = max(i.width for i in imgs); H = sum(i.height for i in imgs)
    sheet = Image.new('RGB', (W, H), (58, 62, 72)); y = 0
    for i in imgs: sheet.paste(i, (0, y)); y += i.height
    sheet.save(sys.argv[1] if len(sys.argv) > 1 else '/tmp/sprites.png')
