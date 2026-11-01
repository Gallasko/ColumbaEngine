#!/usr/bin/env python3
"""Builds the base mod's item looks (mods/base/models/look_*.vxl.json): what a shop item shows on the
player who has it, worn on the body. Their voxels are half the player's (0.0625 m).

Run it again after a tweak: python3 examples/VoxelArena/tools/make_looks.py
It prints each item's "look" for mods/base/upgrades.json: where the piece's bottom centre goes on the
player model (in its voxels: x 0..7 from its right arm to its left, y 0..15 up, z 0..5 back to front).
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

    # Plated: a breastplate, rivets at its corners
    plate = Model().box(0, 0, 0, 7, 5, 0, STEEL).box(3, 0, 0, 4, 5, 0, STEEL_LIGHT)
    for x, y in ((0, 0), (7, 0), (0, 5), (7, 5)):
        plate.at(x, y, 0, STEEL_DARK)
    m["look_plate"] = plate

    # Leech: a cord with fangs hanging from it
    fangs = Model().box(0, 2, 0, 7, 2, 0, LEATHER_DARK)
    for x, low in ((1, 1), (3, 0), (4, 0), (6, 1)):
        fangs.box(x, low, 0, x, 1, 0, BONE)
    m["look_fangs"] = fangs

    # Second wind: a green scarf round the neck, its end down the back
    scarf = Model().ring(12, 8, 4, 5, (76, 184, 106)).box(0, 5, 0, 11, 5, 0, (52, 140, 78))
    scarf.box(8, 0, 0, 9, 3, 0, (76, 184, 106)).box(8, 0, 0, 9, 0, 0, (52, 140, 78))
    m["look_scarf"] = scarf

    # Frenzy: a red headband, two ends at the back
    band = Model().ring(12, 12, 1, 1, (200, 60, 52)).box(6, 0, 0, 7, 0, 0, (160, 40, 36))
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

    # Keen edge: a spiked bracer on the right forearm
    bracer = Model().ring(4, 8, 0, 1, LEATHER).box(0, 0, 0, 3, 0, 0, LEATHER_DARK).box(0, 0, 7, 3, 0, 7, LEATHER_DARK)
    m["look_bracer"] = Model()
    for (x, y, z), c in bracer.cells.items():
        m["look_bracer"].at(x + 1, y, z, c)   # room for the spikes on the outside
    m["look_bracer"].box(0, 0, 2, 0, 1, 2, STEEL_LIGHT).box(0, 0, 5, 0, 1, 5, STEEL_LIGHT)

    # Mending: a bandage on the left forearm, a green leaf on it
    m["look_bandage"] = Model().ring(4, 8, 0, 2, (236, 230, 210)).at(3, 1, 3, (76, 184, 106)).at(3, 1, 4, (76, 184, 106))

    # Swift: knee guards
    m["look_knee"] = Model().box(0, 0, 0, 3, 1, 0, (180, 140, 90)).box(1, 0, 0, 2, 1, 0, (210, 172, 118))

    # Nimble: little wings at the heels
    wing = Model().box(0, 0, 0, 0, 0, 2, (240, 240, 246)).box(0, 1, 0, 0, 1, 1, (240, 240, 246)).at(0, 2, 0, (200, 210, 230))
    m["look_wing"] = wing

    return m


# Each item's pieces: (model, part, the player voxel where the model's corner 0,0,0 goes)
PIECES = {
    "tough":       [("look_pauldron", "arm_r", (-0.5, 11.0, 1.5)), ("look_pauldron", "arm_l", (6.0, 11.0, 1.5))],
    "plated":      [("look_plate", "body", (1.5, 7.0, 4.0))],
    "leech":       [("look_fangs", "body", (1.5, 9.0, 4.25))],
    "second_wind": [("look_scarf", "body", (0.5, 8.25, 0.5))],
    "frenzy":      [("look_headband", "head", (0.5, 12.5, -0.5))],
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
    "mending":     [("look_bandage", "arm_l", (5.5, 6.0, 0.5))],
    "swift":       [("look_knee", "leg_r", (1.0, 3.0, 4.0)), ("look_knee", "leg_l", (4.0, 3.0, 4.0))],
    "nimble":      [("look_wing", "leg_r", (0.5, 1.0, 1.0)), ("look_wing", "leg_l", (6.0, 1.0, 1.0))],
}


def main():
    models = build()
    for name, model in models.items():
        model.save(name)

    for item, pieces in PIECES.items():
        looks = []
        for model, part, corner in pieces:
            p = models[model].pivot()
            at = [corner[i] + p[i] * 0.5 for i in range(3)]
            looks.append('{ "model": "%s", "part": "%s", "at": [%g, %g, %g] }' % (model, part, at[0], at[1], at[2]))
        print('%s: %s' % (item, looks[0] if len(looks) == 1 else "[" + ", ".join(looks) + "]"))


if __name__ == "__main__":
    main()
