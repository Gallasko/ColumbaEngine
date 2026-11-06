#!/usr/bin/env python3
"""Builds the base mod's item looks (mods/base/models/look_*.vxl.json): what a shop item shows on the
player who has it, worn on the body. Their voxels are half the player's (0.0625 m), or a quarter (FINE) for a
piece that must hug the body closer.

Run it again after a tweak: python3 examples/VoxelArena/tools/make_looks.py
It prints each item's "look" for mods/base/upgrades.json (with "scale" for a piece of finer voxels): where the
piece's bottom centre goes on the player model (in its voxels: x 0..7 from its right arm to its left, y 0..15 up, z 0..5 back to front).
"""

import json
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "mods", "base", "models")

STEEL, STEEL_LIGHT, STEEL_DARK = (150, 158, 172), (196, 204, 214), (84, 90, 104)
GOLD, GOLD_DARK = (222, 182, 70), (150, 112, 40)
LEATHER, LEATHER_DARK = (122, 80, 46), (84, 52, 30)
BONE, BONE_TIP = (232, 220, 192), (70, 60, 52)


class Model:
    def __init__(self):
        self.cells = {}

    def box(self, x0, y0, z0, x1, y1, z1, colour):
        """Fills x0..x1, y0..y1, z0..z1 (inclusive)."""
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for z in range(z0, z1 + 1):
                    self.cells[(x, y, z)] = colour
        return self

    def ring(self, w, d, y0, y1, colour):
        """A frame w wide and d deep, one voxel thick, its hole in the middle."""
        for x in range(w):
            for z in range(d):
                if x in (0, w - 1) or z in (0, d - 1):
                    self.box(x, y0, z, x, y1, z, colour)
        return self

    def at(self, x, y, z, colour):
        self.cells[(x, y, z)] = colour
        return self

    def size(self):
        return [max(c[i] for c in self.cells) + 1 for i in range(3)]

    def pivot(self):
        """The bottom centre of the occupied cells, as the game takes it (VoxelModel::pivot)."""
        lo = [min(c[i] for c in self.cells) for i in range(3)]
        hi = [max(c[i] for c in self.cells) + 1 for i in range(3)]
        return [(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2]

    def save(self, name):
        palette = sorted(set(self.cells.values()))
        w, h, d = self.size()
        data = {
            "version": 1,
            "canvas": {"w": w, "h": h, "d": d},
            "palette": [list(c) + [255] for c in palette],
            "layers": [{"name": "body", "visible": True}],
            "cells": [{"x": x, "y": y, "z": z, "c": palette.index(c), "l": 0} for (x, y, z), c in sorted(self.cells.items())],
        }
        with open(os.path.join(OUT, name + ".vxl.json"), "w") as f:
            json.dump(data, f, separators=(",", ":"))


def build():
    m = {}

    # Tough: a steel pad on each shoulder
    m["look_pauldron"] = Model().box(0, 0, 0, 2, 0, 3, STEEL).box(0, 1, 1, 2, 1, 2, STEEL_LIGHT).box(0, 0, 0, 2, 0, 0, STEEL_DARK).box(0, 0, 3, 2, 0, 3, STEEL_DARK)

    # Plated: a breastplate, rivets at its corners (it ends under Leech's cord)
    plate = Model().box(0, 0, 0, 7, 4, 0, STEEL).box(3, 0, 0, 4, 4, 0, STEEL_LIGHT)
    for x, y in ((0, 0), (7, 0), (0, 4), (7, 4)):
        plate.at(x, y, 0, STEEL_DARK)
    m["look_plate"] = plate

    # Leech: a cord across the chest with fangs hanging from it. Two voxels deep: over the breastplate it still
    # stands out
    fangs = Model().box(0, 2, 0, 9, 2, 1, LEATHER_DARK)
    for x, low in ((2, 1), (4, 0), (5, 0), (7, 1)):
        fangs.box(x, low, 0, x, 1, 1, BONE)
    m["look_fangs"] = fangs

    # Second wind: a green scarf across the top of the chest and the back, its end down the back. Not round the
    # sides: there the arms are, and a swinging arm would show the scarf inside it
    green, green_dark = (76, 184, 106), (52, 140, 78)
    scarf = Model().box(0, 4, 0, 9, 5, 0, green).box(0, 4, 7, 9, 5, 7, green)
    scarf.box(0, 5, 0, 9, 5, 0, green_dark).box(0, 5, 7, 9, 5, 7, green_dark)
    scarf.box(8, 0, 0, 9, 3, 0, green).box(8, 0, 0, 9, 0, 0, green_dark)   # the end, beside Magnet's horseshoe
    m["look_scarf"] = scarf

    # Frenzy: a red headband, two ends at the back. Fine voxels: in half ones it stood out round the head like a brim
    band = Model().ring(22, 22, 2, 3, (200, 60, 52)).box(9, 0, 0, 10, 1, 0, (160, 40, 36)).box(12, 0, 0, 13, 1, 0, (160, 40, 36))
    m["look_headband"] = band

    # Horde: horns
    horns = Model()
    for x0, out in ((2, -1), (11, 1)):
        horns.box(x0, 0, 0, x0, 1, 1, BONE).box(x0 + out, 2, 0, x0 + out, 3, 1, BONE).box(x0 + 2 * out, 4, 0, x0 + 2 * out, 4, 1, BONE_TIP)
    m["look_horns"] = horns

    # Focus: a blue gem set in gold, on the forehead
    m["look_gem"] = Model().box(0, 0, 0, 3, 1, 0, GOLD).box(1, 0, 0, 2, 1, 0, (70, 140, 255))

    # Precise: a gold monocle
    m["look_monocle"] = Model().box(0, 0, 0, 2, 2, 0, GOLD).at(1, 1, 0, (200, 230, 255))

    # Quick hands: an hourglass on the belt
    m["look_hourglass"] = Model().box(0, 0, 0, 2, 0, 0, GOLD_DARK).box(0, 3, 0, 2, 3, 0, GOLD_DARK).at(1, 1, 0, (240, 214, 140)).at(1, 2, 0, (200, 230, 255))

    # Potent: a purple vial on the belt
    m["look_vial"] = Model().box(0, 0, 0, 1, 1, 0, (150, 70, 210)).box(0, 2, 0, 1, 2, 0, LEATHER)

    # Greed: a coin pouch, a coin showing
    m["look_pouch"] = Model().box(0, 0, 0, 2, 1, 0, LEATHER).box(0, 2, 0, 2, 2, 0, LEATHER_DARK).at(1, 1, 0, GOLD)

    # Wellspring: a blue flask at the back of the belt
    m["look_flask"] = Model().box(0, 0, 0, 1, 1, 0, (60, 120, 230)).box(0, 2, 0, 1, 2, 0, (200, 230, 255))

    # Magnet: a horseshoe magnet on the back
    magnet = Model().box(0, 0, 0, 3, 0, 0, (200, 50, 46)).box(0, 1, 0, 0, 3, 0, (200, 50, 46)).box(3, 1, 0, 3, 3, 0, (200, 50, 46))
    magnet.at(0, 3, 0, STEEL_LIGHT).at(3, 3, 0, STEEL_LIGHT)
    m["look_magnet"] = magnet

    # Scholar: a book at the back
    m["look_book"] = Model().box(0, 0, 0, 3, 2, 1, (60, 90, 170)).box(1, 0, 0, 3, 0, 1, (236, 230, 210)).box(0, 0, 0, 0, 2, 1, (40, 60, 120)).at(2, 2, 0, GOLD)

    # The rings round a forearm are open on the body's side: the arm is against the body there, and a wall on that
    # side would be inside the body (and come out of it as the arm swings)

    # Keen edge: a spiked bracer on the right forearm
    bracer = Model().ring(4, 8, 0, 1, LEATHER).box(0, 0, 0, 3, 0, 0, LEATHER_DARK).box(0, 0, 7, 3, 0, 7, LEATHER_DARK)
    m["look_bracer"] = Model()
    for (x, y, z), c in bracer.cells.items():
        if x < 3:
            m["look_bracer"].at(x + 1, y, z, c)   # room for the spikes on the outside
    m["look_bracer"].box(0, 0, 2, 0, 1, 2, STEEL_LIGHT).box(0, 0, 5, 0, 1, 5, STEEL_LIGHT)

    # Mending: a bandage on the left forearm, a green leaf on it
    bandage = Model().ring(4, 8, 0, 2, (236, 230, 210)).at(3, 1, 3, (76, 184, 106)).at(3, 1, 4, (76, 184, 106))
    m["look_bandage"] = Model()
    for (x, y, z), c in bandage.cells.items():
        if x > 0:
            m["look_bandage"].at(x - 1, y, z, c)

    # Swift: knee guards
    m["look_knee"] = Model().box(0, 0, 0, 3, 1, 0, (180, 140, 90)).box(1, 0, 0, 2, 1, 0, (210, 172, 118))

    # Nimble: little wings at the ankles, over the anklets of Ember trail
    wing = Model().box(0, 0, 0, 0, 0, 2, (240, 240, 246)).box(0, 1, 0, 0, 1, 1, (240, 240, 246)).at(0, 2, 0, (200, 210, 230))
    m["look_wing"] = wing

    # Glass cannon: a red glass heart on the chest, a crack through it. Three voxels deep: it stands out of the
    # breastplate and of Leech's fangs (at the same depth, its face and theirs would be one surface, and flicker)
    red, crack = (230, 60, 70), (255, 190, 200)
    heart = Model().box(0, 1, 0, 3, 2, 2, red).box(1, 0, 0, 2, 0, 2, red).box(0, 3, 0, 0, 3, 2, red).box(3, 3, 0, 3, 3, 2, red)
    heart.at(1, 2, 2, crack).at(2, 1, 2, crack)
    m["look_heart"] = heart

    # Juggernaut: dark iron greaves on the shins (over the anklets, under the knee guards), a ridge down each
    m["look_greaves"] = Model().box(0, 0, 0, 3, 2, 0, (70, 72, 80)).box(1, 0, 0, 2, 2, 0, (104, 106, 116)).box(0, 2, 0, 3, 2, 0, (50, 52, 58))

    # Cursed coin: a purple coin hanging from an ear, beside the head and over the shoulder pad
    m["look_coin"] = Model().at(0, 2, 1, GOLD_DARK).box(0, 0, 0, 0, 1, 2, (120, 60, 160)).at(0, 0, 1, (200, 140, 255))

    # Overclock: a brass gear on the left upper arm, turned to the side, a tooth out of each edge
    gear = Model().box(0, 1, 1, 0, 4, 4, GOLD).box(0, 2, 2, 0, 3, 3, GOLD_DARK)
    for y, z in ((1, 1), (1, 4), (4, 1), (4, 4)):
        gear.cells.pop((0, y, z), None)
    for y, z in ((0, 2), (5, 3), (2, 0), (3, 5)):
        gear.at(0, y, z, GOLD)
    m["look_gear"] = gear

    # Thorns: green vine round the right upper arm (open on the body's side, as the forearm rings), light thorns
    # sticking out of it: to the outside, the front and the back, two up
    vine, thorn = (70, 120, 50), (150, 200, 90)
    thorns = Model().box(1, 0, 1, 1, 0, 8, vine).box(1, 0, 1, 3, 0, 1, vine).box(1, 0, 8, 3, 0, 8, vine)
    for x, y, z in ((0, 0, 3), (0, 0, 6), (2, 0, 9), (2, 0, 0), (1, 1, 2), (3, 1, 8)):
        thorns.at(x, y, z, thorn)
    m["look_thorns"] = thorns

    # Ember trail: glowing anklets, in voxels a quarter of the player's (FINE): just round the leg, a hair bigger
    # than it (the leg's own size, they would be one surface with it)
    anklet = Model().ring(10, 14, 0, 1, (90, 50, 30))
    for x, z, c in ((0, 4, (255, 130, 40)), (9, 9, (255, 130, 40)), (3, 13, (255, 180, 80)), (6, 0, (255, 180, 80))):
        anklet.at(x, 0, z, c).at(x, 1, z, c)
    m["look_anklet"] = anklet

    return m


FINE = 0.03125   # a piece's voxels in m when they are a quarter of the player's (0.0625, half, by default)

# Each item's pieces: (model, part, the player voxel where the model's corner 0,0,0 goes[, its voxels in m])
PIECES = {
    "tough":       [("look_pauldron", "arm_r", (-0.5, 11.0, 1.5)), ("look_pauldron", "arm_l", (6.0, 11.0, 1.5))],
    "plated":      [("look_plate", "body", (1.5, 7.0, 4.0))],
    "leech":       [("look_fangs", "body", (1.0, 8.5, 4.0))],
    "second_wind": [("look_scarf", "body", (1.0, 8.0, 0.5))],
    "frenzy":      [("look_headband", "head", (0.75, 12.5, -0.25), FINE)],
    "horde":       [("look_horns", "head", (0.0, 15.0, 2.0))],
    "focus":       [("look_gem", "head", (2.5, 13.75, 5.0))],
    "precise":     [("look_monocle", "head", (1.75, 11.5, 5.0))],
    "quick":       [("look_hourglass", "body", (4.25, 5.0, 4.0))],
    "potent":      [("look_vial", "body", (1.25, 5.25, 4.0))],
    "greed":       [("look_pouch", "body", (2.5, 5.25, 4.0))],
    "wellspring":  [("look_flask", "body", (4.75, 5.0, 0.5))],
    "magnet":      [("look_magnet", "body", (2.5, 8.0, 0.5))],
    "scholar":     [("look_book", "body", (2.5, 6.25, 0.0))],
    "keen":        [("look_bracer", "arm_r", (-1.0, 6.0, 0.5))],
    "mending":     [("look_bandage", "arm_l", (6.0, 6.0, 0.5))],
    "swift":       [("look_knee", "leg_r", (1.0, 3.0, 4.0)), ("look_knee", "leg_l", (4.0, 3.0, 4.0))],
    "nimble":      [("look_wing", "leg_r", (0.5, 1.5, 1.0)), ("look_wing", "leg_l", (6.0, 1.5, 1.0))],
    "glass_cannon":[("look_heart", "body", (3.75, 8.25, 4.0))],
    "juggernaut":  [("look_greaves", "leg_r", (1.0, 1.5, 4.0)), ("look_greaves", "leg_l", (4.0, 1.5, 4.0))],
    "cursed_coin": [("look_coin", "head", (0.5, 11.5, 1.75))],
    "overclock":   [("look_gear", "arm_l", (7.0, 8.0, 0.5))],
    "thorns":      [("look_thorns", "arm_r", (-1.0, 8.5, 0.0))],
    "ember_trail": [("look_anklet", "leg_r", (0.75, 1.0, 0.75), FINE), ("look_anklet", "leg_l", (3.75, 1.0, 0.75), FINE)],
}


def main():
    models = build()
    for name, model in models.items():
        model.save(name)

    for item, pieces in PIECES.items():
        looks = []
        for model, part, corner, *scale in pieces:
            p = models[model].pivot()
            k = (scale[0] if scale else 0.0625) / 0.125   # a piece voxel in player voxels
            at = [corner[i] + p[i] * k for i in range(3)]
            extra = ', "scale": %g' % scale[0] if scale else ""
            looks.append('{ "model": "%s", "part": "%s", "at": [%g, %g, %g]%s }' % (model, part, at[0], at[1], at[2], extra))
        print('%s: %s' % (item, looks[0] if len(looks) == 1 else "[" + ", ".join(looks) + "]"))


if __name__ == "__main__":
    main()
