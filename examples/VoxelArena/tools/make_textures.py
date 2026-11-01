#!/usr/bin/env python3
"""Paints the base mod's block textures (mods/base/textures/*.png), 16 x 16 pixel art.

Run it again after a tweak: python3 examples/VoxelArena/tools/make_textures.py
Every texture is seeded by its name, so a run gives the same pictures each time. A png painted
by hand can simply replace the generated one (and then this script must not be run again, or it
is overwritten).
"""

import math
import os
import random
import struct
import zlib

N = 16
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "mods", "base", "textures")


# ---- png ----------------------------------------------------------------------------------------

def save(name, img):
    """img: N rows of N (r, g, b, a) tuples."""
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in img)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")

    with open(os.path.join(OUT, name + ".png"), "wb") as f:
        f.write(png)


# ---- helpers ------------------------------------------------------------------------------------

def clamp(v):
    return max(0, min(255, int(round(v))))


def shade(c, k, a=255):
    """Colour c times k (k > 1 lightens)."""
    return (clamp(c[0] * k), clamp(c[1] * k), clamp(c[2] * k), a)


def mix(a, b, t):
    return tuple(clamp(a[i] + (b[i] - a[i]) * t) for i in range(3)) + (255,)


def canvas(c=(0, 0, 0, 0)):
    return [[c for _ in range(N)] for _ in range(N)]


def noise(rng, cell=4):
    """Smooth value noise over the tile, 0..1, that tiles (it wraps at the edges)."""
    g = N // cell
    grid = [[rng.random() for _ in range(g)] for _ in range(g)]

    def smooth(t):
        return t * t * (3 - 2 * t)

    out = []
    for y in range(N):
        row = []
        for x in range(N):
            gx, gy = x / cell, y / cell
            x0, y0 = int(gx) % g, int(gy) % g
            x1, y1 = (x0 + 1) % g, (y0 + 1) % g
            tx, ty = smooth(gx - int(gx)), smooth(gy - int(gy))
            top = grid[y0][x0] * (1 - tx) + grid[y0][x1] * tx
            bottom = grid[y1][x0] * (1 - tx) + grid[y1][x1] * tx
            row.append(top * (1 - ty) + bottom * ty)
        out.append(row)
    return out


def speckled(rng, base, spread, cell=4, grain=0.08):
    """A base colour, blotched by noise and grained per pixel."""
    n = noise(rng, cell)
    return [[shade(base, 1 + (n[y][x] - 0.5) * spread + (rng.random() - 0.5) * grain) for x in range(N)] for y in range(N)]


def specks(img, rng, colour, count, size=1):
    for _ in range(count):
        x, y = rng.randrange(N), rng.randrange(N)
        for dy in range(size):
            for dx in range(size):
                img[(y + dy) % N][(x + dx) % N] = colour


# ---- ground -------------------------------------------------------------------------------------

GRASS = (106, 162, 74)
DIRT = (134, 96, 64)


def grass_top(rng):
    img = speckled(rng, GRASS, 0.18, cell=4, grain=0.08)
    # Blades catching the light, and darker roots between them
    specks(img, rng, shade(GRASS, 1.15), 14)
    specks(img, rng, shade(GRASS, 0.8), 12)
    return img


def dirt(rng, base=DIRT):
    img = speckled(rng, base, 0.25, cell=4, grain=0.12)
    specks(img, rng, shade(base, 0.7), 14)
    specks(img, rng, shade(base, 1.25), 8)
    # A few pebbles
    for _ in range(3):
        x, y = rng.randrange(N - 1), rng.randrange(N - 1)
        img[y][x] = (150, 146, 140, 255)
        img[y][x + 1] = (120, 116, 112, 255)
    return img


def grass_side(rng):
    img = dirt(rng)
    top = grass_top(rng)
    # Grass hangs over the edge: two to five pixels per column, a darker lip under it
    for x in range(N):
        depth = 2 + rng.choice([0, 1, 1, 2, 2, 3])
        for y in range(depth):
            img[y][x] = top[y][x]
        img[depth][x] = shade(GRASS, 0.62)
    return img


def sand(rng):
    base = (214, 196, 140)
    img = speckled(rng, base, 0.10, cell=8, grain=0.07)
    # Wind ripples
    for y in range(N):
        for x in range(N):
            if (y + int(2 * math.sin(x * 0.8))) % 5 == 0:
                img[y][x] = shade(img[y][x], 0.93)
    specks(img, rng, shade(base, 0.82), 10)
    specks(img, rng, shade(base, 1.08), 10)
    return img


def earth(rng):
    # Packed and lighter than dirt, in layers
    base = (150, 112, 74)
    img = dirt(rng, base)
    for y in (4, 9, 13):
        for x in range(N):
            if rng.random() < 0.8:
                img[y][x] = shade(base, 0.8)
    return img


def snow(rng):
    base = (232, 237, 244)
    img = speckled(rng, base, 0.06, cell=8, grain=0.03)
    specks(img, rng, (206, 216, 232, 255), 10)
    specks(img, rng, (250, 252, 255, 255), 8)
    return img


def hay(rng):
    base = (214, 184, 96)
    img = canvas()
    # Straws lying across, each its own shade
    for y in range(N):
        k = 0.85 + rng.random() * 0.3
        for x in range(N):
            img[y][x] = shade(base, k + (rng.random() - 0.5) * 0.08)
    for _ in range(18):
        x, y = rng.randrange(N), rng.randrange(N)
        length = rng.randrange(3, 7)
        for i in range(length):
            img[y][(x + i) % N] = shade(base, 1.18)
    specks(img, rng, shade(base, 0.65), 10)
    # The binding
    for x in range(N):
        img[7][x] = (120, 92, 52, 255)
        img[8][x] = (104, 78, 44, 255)
    return img


# ---- stone --------------------------------------------------------------------------------------

def cracks(img, rng, base, count):
    for _ in range(count):
        x, y = rng.randrange(N), rng.randrange(N)
        for _ in range(rng.randrange(3, 7)):
            img[y % N][x % N] = shade(base, 0.68)
            x += rng.choice([-1, 0, 1])
            y += rng.choice([0, 1])


def stone(rng, base=(128, 132, 140), rough=0.22):
    img = speckled(rng, base, rough, cell=4, grain=0.08)
    cracks(img, rng, base, 3)
    specks(img, rng, shade(base, 1.15), 8)
    return img


def bedrock(rng):
    base = (62, 62, 70)
    img = speckled(rng, base, 0.6, cell=4, grain=0.15)
    specks(img, rng, (34, 34, 40, 255), 18, 2)
    specks(img, rng, (96, 96, 104, 255), 8)
    return img


def boulder(rng):
    base = (112, 108, 116)
    img = speckled(rng, base, 0.35, cell=8, grain=0.10)
    specks(img, rng, shade(base, 0.75), 16)
    specks(img, rng, (138, 134, 128, 255), 10)
    return img


def blocks(rng, base, rows, mortar, bevel=True, jitter=0.12):
    """Stone or brick work. rows: list of (height, brick width, offset)."""
    img = canvas()
    y0 = 0
    for height, width, offset in rows:
        for x0 in range(-offset, N, width):
            k = 1 + (rng.random() - 0.5) * jitter * 2
            for y in range(y0, y0 + height):
                for x in range(x0, x0 + width):
                    if not 0 <= x < N:
                        continue
                    if y == y0 + height - 1 or x == x0 + width - 1:
                        img[y][x] = mortar
                        continue
                    c = shade(base, k + (rng.random() - 0.5) * 0.1)
                    if bevel and (y == y0 or x == x0):
                        c = shade(c, 1.12)
                    elif bevel and (y == y0 + height - 2 or x == x0 + width - 2):
                        c = shade(c, 0.9)
                    img[y][x] = c
        y0 += height
    return img


def brick(rng):
    return blocks(rng, (160, 78, 60), [(4, 8, 0), (4, 8, 4), (4, 8, 0), (4, 8, 4)], (188, 178, 164, 255), bevel=False, jitter=0.14)


def arena_brick(rng):
    return blocks(rng, (150, 72, 56), [(4, 8, 2), (4, 8, 6), (4, 8, 2), (4, 8, 6)], (98, 92, 90, 255), jitter=0.12)


def arena_wall(rng):
    img = blocks(rng, (86, 88, 98), [(8, 8, 0), (8, 8, 4)], (58, 58, 66, 255), jitter=0.08)
    cracks(img, rng, (86, 88, 98), 2)
    return img


# ---- made things --------------------------------------------------------------------------------

def planks(rng):
    base = (168, 124, 76)
    img = canvas()
    for board in range(4):
        y0 = board * 4
        k = 0.9 + rng.random() * 0.2
        end = rng.randrange(4, 12)   # where two boards meet along the row
        for y in range(y0, y0 + 4):
            for x in range(N):
                if y == y0 + 3:
                    img[y][x] = shade(base, 0.6)
                elif x == end:
                    img[y][x] = shade(base, 0.7)
                else:
                    grain = 0.06 * math.sin(x * 0.9 + y * 2.3 + board)
                    img[y][x] = shade(base, k + grain + (rng.random() - 0.5) * 0.06)
        # A knot, and nails at the joint
        kx = rng.randrange(N)
        img[y0 + 1][kx] = shade(base, 0.72)
        img[y0 + 1][(end + 1) % N] = (70, 66, 64, 255)
    return img


def gold(rng):
    base = (232, 190, 72)
    img = speckled(rng, base, 0.10, cell=8, grain=0.05)
    for i in range(N):
        img[0][i] = img[i][0] = shade(base, 1.2)
        img[N - 1][i] = img[i][N - 1] = shade(base, 0.72)
    # A glint across
    for i in range(3, 8):
        img[i][i + 2] = (255, 244, 196, 255)
    img[3][12] = img[12][4] = (255, 240, 180, 255)
    return img


def rope(rng):
    base = (196, 170, 112)
    img = canvas()
    for y in range(N):
        for x in range(N):
            strand = (x + y) % 4
            k = [1.12, 1.0, 0.86, 0.72][strand]
            img[y][x] = shade(base, k + (rng.random() - 0.5) * 0.06)
    return img


def ice(rng):
    base = (178, 222, 246)
    img = speckled(rng, base, 0.08, cell=8, grain=0.03)
    for start in (2, 9):
        for i in range(6):
            x, y = start + i, 2 + i + (start // 3)
            if 0 <= x < N and 0 <= y < N:
                img[y][x] = (236, 248, 255, 255)
    for i in range(N):
        img[0][i] = img[i][0] = (214, 240, 252, 255)
        img[N - 1][i] = img[i][N - 1] = shade(base, 0.86)
    return img


# ---- decorations (see-through: drawn crossed in the cell) --------------------------------------

def tall_grass(rng):
    img = canvas()
    for _ in range(16):
        x = rng.randrange(0, N)
        height = rng.randrange(5, 15)
        lean = rng.choice([-1, 0, 0, 1])
        k = 0.8 + rng.random() * 0.45
        for i in range(height):
            y = N - 1 - i
            px = x + (lean * i) // 5
            if 0 <= px < N:
                img[y][px] = shade(GRASS, k * (0.75 + 0.35 * i / height))
    return img


def flower(rng, petal, centre):
    img = canvas()
    # The stem, with a leaf each side
    for y in range(6, N):
        img[y][7] = shade(GRASS, 0.85)
    for x, y in ((6, 11), (5, 10), (8, 12), (9, 11), (10, 10)):
        img[y][x] = shade(GRASS, 1.0 if x > 7 else 0.9)
    # The head
    for dx, dy in ((0, -2), (-2, 0), (2, 0), (0, 2), (-1, -1), (1, -1), (-1, 1), (1, 1), (-1, 0), (1, 0), (0, -1), (0, 1)):
        far = abs(dx) == 2 or abs(dy) == 2
        img[4 + dy][7 + dx] = shade(petal, 0.85 if far else 1.0)
    img[4][7] = centre
    return img


# ---- all ----------------------------------------------------------------------------------------

TEXTURES = {
    "grass_top": grass_top,
    "grass_side": grass_side,
    "dirt": dirt,
    "sand": sand,
    "earth": earth,
    "snow": snow,
    "hay": hay,
    "stone": stone,
    "dark_stone": lambda rng: stone(rng, (86, 88, 98), 0.3),
    "bedrock": bedrock,
    "boulder": boulder,
    "brick": brick,
    "arena_brick": arena_brick,
    "arena_wall": arena_wall,
    "planks": planks,
    "gold": gold,
    "rope": rope,
    "ice": ice,
    "tall_grass": tall_grass,
    "flower_red": lambda rng: flower(rng, (214, 52, 48), (250, 214, 90, 255)),
    "flower_yellow": lambda rng: flower(rng, (246, 206, 60), (196, 120, 40, 255)),
}


def main():
    os.makedirs(OUT, exist_ok=True)

    for name, paint in TEXTURES.items():
        save(name, paint(random.Random(zlib.crc32(name.encode()))))

    print("wrote", len(TEXTURES), "textures to", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
