#!/usr/bin/env python3
"""Builds the voxel models of the enemies added after the first ones (mods/base/models/<name>.vxl.json): the
sapper, the mender, the rimecaller, the golem and its pebbles, and the mender's bolt.

Run it again after a tweak: python3 examples/VoxelArena/tools/make_enemies.py
Each model's layers are its body parts, as the game animates them (see README, "Limbs"): leg_l, leg_r, arm_l,
arm_r, head (four legs: leg_fl, leg_fr, leg_bl, leg_br), and body. x 0 is the model's right side, y is up, and
+z is the front (the face).
"""

import json
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "mods", "base", "models")


class Model:
    def __init__(self):
        self.cells = {}

    def box(self, layer, x0, y0, z0, x1, y1, z1, colour):
        """Fills x0..x1, y0..y1, z0..z1 (inclusive) on a layer."""
        for x in range(min(x0, x1), max(x0, x1) + 1):
            for y in range(min(y0, y1), max(y0, y1) + 1):
                for z in range(min(z0, z1), max(z0, z1) + 1):
                    self.cells[(x, y, z)] = (layer, colour)
        return self

    def at(self, layer, x, y, z, colour):
        self.cells[(x, y, z)] = (layer, colour)
        return self

    def clear(self, x, y, z):
        self.cells.pop((x, y, z), None)
        return self

    def save(self, name):
        layers = []
        for layer, _ in self.cells.values():
            if layer not in layers:
                layers.append(layer)
        palette = sorted(set(c for _, c in self.cells.values()))
        size = [max(c[i] for c in self.cells) + 1 for i in range(3)]
        data = {
            "version": 1,
            "canvas": {"w": size[0], "h": size[1], "d": size[2]},
            "palette": [list(c) + [255] for c in palette],
            "layers": [{"name": l, "visible": True} for l in layers],
            "cells": [{"x": x, "y": y, "z": z, "c": palette.index(c), "l": layers.index(l)} for (x, y, z), (l, c) in sorted(self.cells.items())],
        }
        with open(os.path.join(OUT, name + ".vxl.json"), "w") as f:
            json.dump(data, f, separators=(",", ":"))
        print(name, size, len(self.cells), "voxels", layers)


def sapper():
    """A small hunched runner, a keg of powder on its back and a lit fuse on top."""
    SKIN, SKIN_DARK = (128, 150, 92), (96, 114, 68)
    CLOTH, CLOTH_DARK = (120, 70, 48), (86, 48, 34)
    WOOD, BAND = (150, 104, 56), (70, 60, 56)
    EYE, FUSE, SPARK = (255, 210, 60), (60, 40, 30), (255, 120, 40)

    m = Model()
    # Legs, short, under a tunic
    m.box("leg_r", 1, 0, 3, 2, 3, 4, CLOTH_DARK).box("leg_r", 1, 0, 4, 2, 0, 5, BAND)
    m.box("leg_l", 4, 0, 3, 5, 3, 4, CLOTH_DARK).box("leg_l", 4, 0, 4, 5, 0, 5, BAND)
    # The body, leaning forward, and the keg on its back (low z is the back)
    m.box("body", 1, 4, 2, 5, 7, 5, CLOTH).box("body", 1, 4, 5, 5, 4, 5, CLOTH_DARK)
    m.box("body", 1, 4, 0, 5, 9, 1, WOOD)
    for y in (5, 8):
        m.box("body", 1, y, 0, 5, y, 1, BAND)
    m.at("body", 3, 10, 1, FUSE).at("body", 3, 11, 1, FUSE).at("body", 3, 12, 1, SPARK)
    # Arms hanging forward
    m.box("arm_r", 0, 3, 3, 0, 7, 4, SKIN).at("arm_r", 0, 3, 5, SKIN_DARK)
    m.box("arm_l", 6, 3, 3, 6, 7, 4, SKIN).at("arm_l", 6, 3, 5, SKIN_DARK)
    # The head, pushed forward, with big ears and yellow eyes
    m.box("head", 2, 8, 3, 4, 10, 6, SKIN).box("head", 2, 10, 3, 4, 10, 5, CLOTH)
    m.at("head", 1, 9, 4, SKIN_DARK).at("head", 5, 9, 4, SKIN_DARK)
    m.at("head", 2, 9, 6, EYE).at("head", 4, 9, 6, EYE)
    m.save("sapper")


def robed(name, robe, robe_dark, trim, skin, hood, staff, gem, gem_light, crown=None):
    """A robed caster with a staff in its right hand (the mender, the rimecaller)."""
    m = Model()
    # The robe's skirt, as the legs (it swings with the walk)
    m.box("leg_r", 1, 0, 1, 3, 5, 4, robe).box("leg_r", 1, 0, 1, 3, 0, 4, robe_dark)
    m.box("leg_l", 4, 0, 1, 6, 5, 4, robe).box("leg_l", 4, 0, 1, 6, 0, 4, robe_dark)
    # The body, a trim down the front and a belt
    m.box("body", 1, 6, 1, 6, 10, 4, robe).box("body", 3, 6, 4, 4, 10, 4, trim).box("body", 1, 7, 1, 6, 7, 4, robe_dark)
    # Arms in sleeves, and the staff by the right hand, its gem on top (a layer of its own that stays still: in the
    # arm it would move the shoulder joint, the top of the layer, up to the gem)
    m.box("arm_l", 7, 5, 2, 7, 10, 3, robe).box("arm_l", 7, 5, 2, 7, 5, 3, skin)
    m.box("arm_r", 0, 5, 2, 0, 10, 3, robe).box("arm_r", 0, 5, 2, 0, 5, 3, skin)
    m.box("staff", 0, 1, 4, 0, 14, 4, staff)
    m.box("staff", 0, 15, 4, 0, 16, 4, gem).at("staff", 0, 17, 4, gem_light)
    # The head in a hood, a shadowed face with two eyes
    m.box("head", 1, 11, 0, 6, 15, 4, hood).box("head", 2, 11, 4, 5, 14, 4, (40, 36, 44))
    m.at("head", 2, 13, 4, gem_light).at("head", 5, 13, 4, gem_light)
    if crown:
        for x in (1, 3, 4, 6):
            m.at("head", x, 16, 2, crown)
        m.at("head", 3, 17, 2, crown).at("head", 4, 17, 2, crown)
    m.save(name)


def golem():
    """A heap of mossy stone on two stumps, its cracks glowing (lightning finds them)."""
    STONE, STONE_LIGHT, STONE_DARK = (128, 124, 120), (160, 156, 150), (90, 86, 84)
    MOSS, CRACK, EYE = (88, 120, 64), (120, 220, 255), (150, 240, 255)

    m = Model()
    m.box("leg_r", 3, 0, 2, 5, 5, 5, STONE_DARK).box("leg_r", 3, 0, 5, 5, 1, 6, STONE_DARK)
    m.box("leg_l", 7, 0, 2, 9, 5, 5, STONE_DARK).box("leg_l", 7, 0, 5, 9, 1, 6, STONE_DARK)
    # The body: a boulder of a chest, moss on the shoulders, a glowing crack across it
    m.box("body", 2, 6, 1, 10, 13, 6, STONE).box("body", 3, 12, 2, 9, 14, 5, STONE_LIGHT)
    m.box("body", 2, 13, 1, 4, 13, 4, MOSS).box("body", 8, 13, 1, 10, 13, 4, MOSS)
    for x, y in ((4, 11), (5, 10), (6, 10), (7, 9), (8, 9)):
        m.at("body", x, y, 6, CRACK)
    # Arms two wide, ending in big fists
    m.box("arm_r", 0, 6, 2, 1, 13, 5, STONE).box("arm_r", 0, 3, 2, 1, 5, 6, STONE_DARK)
    m.box("arm_l", 11, 6, 2, 12, 13, 5, STONE).box("arm_l", 11, 3, 2, 12, 5, 6, STONE_DARK)
    m.at("arm_r", 0, 9, 5, CRACK).at("arm_l", 12, 9, 5, CRACK)
    # A small head sunk between the shoulders
    m.box("head", 4, 15, 2, 8, 17, 6, STONE_LIGHT).box("head", 4, 15, 6, 8, 15, 6, STONE_DARK)
    m.at("head", 5, 16, 6, EYE).at("head", 7, 16, 6, EYE)
    m.save("golem")


def pebble():
    """A small round rock on four stubby legs, two glowing eyes."""
    STONE, STONE_LIGHT, STONE_DARK, EYE = (134, 130, 124), (168, 162, 156), (92, 88, 86), (150, 240, 255)

    m = Model()
    for name, x, z in (("leg_fr", 0, 3), ("leg_fl", 4, 3), ("leg_br", 0, 0), ("leg_bl", 4, 0)):
        m.box(name, x, 0, z, x, 0, z + 1, STONE_DARK)
    m.box("body", 0, 1, 0, 4, 3, 4, STONE).box("body", 1, 4, 1, 3, 4, 3, STONE_LIGHT)
    for x, y, z in ((0, 1, 0), (4, 1, 0), (0, 3, 0), (4, 3, 0), (0, 3, 4), (4, 3, 4)):
        m.clear(x, y, z)
    m.at("body", 1, 2, 4, STONE_DARK).at("body", 3, 2, 4, STONE_DARK)
    m.at("body", 1, 3, 4, EYE).at("body", 3, 3, 4, EYE)
    m.save("pebble")


def mend_orb():
    """The mender's bolt: a small green orb with a bright core."""
    GREEN, LIGHT = (110, 220, 130), (210, 255, 220)
    m = Model()
    for x in range(3):
        for y in range(3):
            for z in range(3):
                if (x == 1) + (y == 1) + (z == 1) >= 2:
                    m.at("body", x, y, z, LIGHT if (x, y, z) == (1, 1, 1) else GREEN)
    m.save("mend_orb")


if __name__ == "__main__":
    sapper()
    robed("mender", robe=(226, 222, 206), robe_dark=(176, 170, 152), trim=(110, 200, 120), skin=(214, 176, 140),
          hood=(110, 170, 110), staff=(120, 86, 52), gem=(110, 220, 130), gem_light=(200, 255, 210))
    robed("rimecaller", robe=(120, 156, 204), robe_dark=(84, 110, 156), trim=(220, 240, 255), skin=(196, 210, 230),
          hood=(170, 200, 236), staff=(200, 220, 240), gem=(150, 215, 255), gem_light=(230, 248, 255), crown=(200, 240, 255))
    golem()
    pebble()
    mend_orb()
