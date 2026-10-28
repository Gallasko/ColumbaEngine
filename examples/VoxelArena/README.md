# Voxel Arena 0.1

A small voxel arena to find the combat feel of the voxel RPG: run, jump, roll, and fight a training
dummy and one enemy (the brute) with a sword. A second mod, `testmod`, adds a spear without any C++.

All the game content lives in `mods/`. The game's own content is the `base` mod, written exactly like
any other mod. Every gameplay number is in a data file and reloads with F5.

## Build and run

```sh
cd release
make -j18 VoxelArena test_voxelarena
cd ..
./release/VoxelArena            # or ./make_and_play.sh VoxelArena
./release/test_voxelarena       # 49 tests, about half a second
```

The game reads its mods and its shader straight from the source tree (`examples/VoxelArena/mods`,
`examples/VoxelArena/shader`), so editing a file and pressing F5 needs no rebuild. It looks for the
engine's `res/` and `shader/` in the working directory, and moves to the repository root by itself
when they are not there.

Options:

| Option | What it does |
|---|---|
| `--mods <dir>` | Load mods from another folder |
| `--exec "<commands>"` | Run console commands at start, joined with `;` (see Console) |

## Controls

| Key | Action |
|---|---|
| WASD | Move (relative to the camera) |
| Mouse | Turn the camera; wheel zooms |
| Space | Jump (hold for a higher jump) |
| Left Shift | Dodge roll; keep holding to sprint out of the roll |
| Left mouse / J | Light attack (3-hit combo) |
| Right mouse / K | Heavy attack |
| 1, 2 ... | Pick a weapon (1 = sword, 2 = spear from testmod) |
| V | Third person / first person |
| Q | Raise a wall of earth in front of you |
| E | Dig a pit where the crosshair is |
| F / middle mouse (hold) | Mine the block under the crosshair |
| F1 | Debug overlay |
| F2 | Hitboxes (red: active, orange: winding up) and hurtboxes (green, blue while invulnerable) |
| F5 | Reload all content |
| `` ` `` | Console |
| Esc | Free or capture the mouse (hold Left Alt for a moment) |

Bindings are data: the `controls` section of `mods/base/tuning.json`.

## How a fight works

- **Stamina** pays for rolls, attacks and sprinting. It refills after a short delay. Hitting zero
  makes you *exhausted*: no sprint, slower refill.
- **The dodge roll** covers about 4.5 m and has invulnerable frames (16 of its 27 frames by default).
  You can roll through enemies while invulnerable.
- **Movement help**, all in the `player` section of the tuning:
  - *Ledge assist*: a jump that ends with the feet just under a ledge (`mantleHeight`) climbs onto it.
  - *Corner correction*: a head that clips a block edge on the way up slides past it (`cornerCorrection`).
  - *Momentum*: the takeoff speed is kept in the air and for a moment after landing (`landingGrace`), so
    sprint jumps and roll jumps chain; extra speed then bleeds off (`overspeedDecel`).
  - A jump cancels the roll from `dodge.jumpFrom` on (its recovery).
- **Landings**: a fall faster than `hardLandingSpeed` slows you for `hardLandingFrames` (to
  `hardLandingMoveSpeed` of the walk speed; 0 makes it a root). A dodge pressed in the air up to
  `landingRollWindow` seconds before landing rolls on touchdown instead. Knocked into the air by a
  hit, the same dodge gets you up at once (wake-up roll).
- **The camera under a roof** comes down rather than in: when a ceiling blocks it, it lowers until it
  sees the player from its full distance, then eases back up. A wall behind you still pulls it in.
- **First person** (V): the body faces where you look, so you strafe instead of turning, and attacks
  go where you look (no aim assist; the swing follows the view while it runs). The weapon is drawn in front of the eye with the same swing data; the view
  dips during a roll. Its numbers are the `firstPerson` section of the tuning.
- **Attacks** have startup, active and recovery frames. Hitboxes exist only during active frames.
  A light press during an attack is remembered and starts the next attack of the combo once the
  attack allows it (`chainFrom`). A dodge can cancel an attack from `dodgeCancelFrom` on. A light
  press at the end of a roll becomes the dodge attack.
- **Poise**: every hit lowers the target's poise. At zero the target is staggered (it cannot act),
  then its poise refills. An attack in progress can add armor, so the brute's slam is not
  interrupted by a jab.
- **Hit feedback**: hitstop (both fighters freeze for a few frames), knockback, a white flash on the
  target, camera shake.
- **The brute** tracks you while it winds up its slam (it glows orange), then commits. Roll late,
  then punish its long recovery. A heavy attack or a full light combo breaks its poise.

## The enemies

Each one is a JSON file in `mods/base/enemies/` and a pgscript brain in `mods/base/scripts/`.

| Enemy | Role | How it fights |
|---|---|---|
| Brute | Tank | Slow, tough, one big slam with a long wind-up. Smashes walls in its way. |
| Archer | Ranged | Keeps 6 to 14 m away, backs off when you close in, moves sideways to find a line of fire when a block is in the way, shoots sooner while you are busy. |
| Skulker | Flanker | Fast and weak. Goes round to your back while someone else holds your attention, bites when you cannot see it, backs off after each bite. |

Scripts work as a group through what the game puts in `me`: `alliesNear`, `alliesAttacking`,
`alliesNearTarget` (other enemies close to you) and `targetFacing` (0 = in front of you, 180 =
behind). A script answers with `me.move` (`hold`, `chase`, `strafe`, `retreat`, `flank`), `me.attack`,
and for `flank` the distance it keeps, `me.radius`. The full list is at the top of `brute.pg`.

**Projectiles** are part of an attack: a `projectile` object (`speed`, `gravity`, `lead`, `damage`,
`poiseDamage`, `terrainDamage`, `spawn`, `model`...) fires on the attack's first active frame. An
enemy aims at your chest, raised for the arc and leading you by `lead`. A roll's invulnerable frames
let an arrow through; a block stops it (and takes `terrainDamage`), so a raised wall is cover.

## Sounds, particles and animation

- **Effects** (`mods/base/effects.json`): for each game event (`hit`, `stagger`, `kill`, `swing`,
  `windup`, `shoot`, `arrow_hit`, `dodge`, `jump`, `land`, `hard_land`, `step`, `chip`, `break`,
  `raise`, `dig`, `piece_fall`, `piece_land`), the sound it plays, the particles it throws and the
  camera shake.
- **Sounds** (`mods/base/sounds.json`) are made from numbers when the game starts: a tone and noise,
  a pitch glide, attack and decay, a low-pass filter. No audio file is in the repository. A mod can
  give any sound as a file instead: `"hit": { "file": "sounds/hit.wav" }`. Sounds get quieter past
  10 m and silent past 32 m.
- **Limbs**: a model's editor layers are its body parts. Layers named `leg_l`, `leg_r`, `arm_l`,
  `arm_r` and `head` (four legs: `leg_fl`, `leg_fr`, `leg_bl`, `leg_br`) are animated: a walk cycle
  with footsteps, arms that follow the attack, a tuck in the roll, a recoil in a stagger. Any other
  layer is part of the body. A model with a single layer stays rigid.

## Terrain as a weapon

The arena can be broken, and breaking it is part of the fight.

- **Blocks** have `hardness` (the damage it takes to break them) and `breakable`. The floor's bedrock
  and the outer walls are unbreakable. A damaged block darkens as it gets close to breaking.
- **Abilities** (`mods/base/abilities/`, data like the rest; the player's are listed in
  `player.abilities`): `raise_wall` puts up a short-lived earth wall, `dig_pit` digs a pit that fills
  back in later (lifting whoever is inside), `mine` chips at the aimed block. They aim with the
  crosshair; the block it is on is outlined when an ability can reach it.
- **Collapse**: blocks that are no longer connected to the ground (or to an unbreakable block) fall as
  one piece and crush what they land on (`terrain` section of the tuning). The arena has a trap for
  it: a boulder hanging from a rope between two pillars, near the centre. Stand about 5 m away, aim
  up at the rope over the boulder and mine it.
- **Attacks break terrain** with `terrainDamage`: the brute's slam goes through a raised wall, and its
  script slams on purpose when a block stands between it and you (`me.blocked`). The sword's heavy
  breaks earth too.
- An enemy that falls in a pit cannot step out (step-up is one block); the pit fills back in after
  `lifetime` seconds.

## Layout

```
examples/VoxelArena/
  Core/          the game, without windows or GL (the tests use it directly)
    json         JSON reader that keeps line numbers, for error messages
    voxelgrid    the voxel grid: a flat array of block ids (seed of the future chunk)
    mesher       one mesh for a grid, with face culling and corner darkening
    physics      a box against the grid, moved one axis at a time, auto step-up
    content      mods, load order, registries, overrides, patches, typed definitions
    tuning       every gameplay number, read from tuning.json
    world        the simulation: player, enemies, attacks, dodges, hits (60 steps per second)
    terrain      block damage, blocks that last a while, collapse and falling pieces
    scriptbrain  enemy behaviour in pgscript, one sandboxed VM per script
    orbitcamera  the third-person camera: smoothing, wall pull-in, shake
  Game/          the engine side: input, drawing, HUD, console
  shader/        the voxel shader
  mods/base/     the game's content
  mods/testmod/  the spear
test/voxelarena/ the tests
```

From VoxelTest it keeps the `.vxl.json` model format (models stay editable in its editor, which a
test checks) and the mouse capture handling (including the WSL case). It does not keep the
one-entity-per-voxel renderer: the arena is one mesh, and each model is one mesh drawn once per
fighter.

## Making a mod

A mod is a folder in `mods/` with a `mod.json`:

```json
{
    "id": "mymod",
    "name": "My mod",
    "version": "0.1.0",
    "api": 1,
    "dependencies": ["base"]
}
```

Mods load after their dependencies (`loadAfter` orders a mod after another one without needing it).
A mod with a missing dependency, or in a dependency cycle, is skipped and reported. `api` is the
content API version the mod was written for. The API is unstable: a mismatch is only a warning.

### What a mod can hold

| File | Defines | ID |
|---|---|---|
| `blocks.json` | blocks: `color`, `solid`, `colorNoise`, `hardness`, `breakable` | `mymod:<key>` |
| `abilities/<name>.json` | a terrain ability (`kind`: wall, pit, mine) | `mymod:<name>` |
| `weapons/<name>.json` | a weapon and its moveset | `mymod:<name>` |
| `enemies/<name>.json` | an enemy: stats, moveset, script, params | `mymod:<name>` |
| `models/<name>.vxl.json` | a voxel model (saved by the VoxelTest editor) | `mymod:<name>` |
| `scripts/<name>.pg` | an enemy brain | `mymod:<name>` |
| `tuning.json` | numbers merged over the tuning of earlier mods | |
| `arena.json` | the arena (the last mod with one wins) | |
| `patches/*.json` | changes to content of other mods | |

IDs are always `namespace:name`. Inside a mod, a name without a namespace means the mod's own
(`"model": "spear"` in testmod is `testmod:spear`); use the full ID to point at another mod's
content (`"model": "base:sword"`).

**Replacing** a definition: give the same ID. A file `weapons/sword.json` in your mod with
`"id": "base:sword"` replaces the base sword.

**Patching** a definition: a file in `patches/` with one patch or a list of them. Objects merge key
by key, anything else is replaced:

```json
[
    { "kind": "enemy",  "target": "base:brute", "set": { "health": 300, "walkSpeed": 3.5 } },
    { "kind": "tuning", "set": { "player": { "walkSpeed": 7 } } }
]
```

`kind` is `block`, `weapon`, `enemy`, `tuning` or `arena`. Patches apply after every mod is read,
in load order.

### Attacks

See `mods/base/weapons/sword.json`: the header comment explains every field. In short:

- `startup`, `active`, `recovery`: frames at 60 per second.
- `damage`, `poiseDamage`, `staminaCost`, `knockback`, `knockup`, `hitstop`, `shake`.
- `hitboxes`: boxes in the attacker's space (x right, y up from the feet, z forward).
- `lunge`, `lungeFrom`, `lungeTo`: forward speed during those frames.
- `turnRate`: degrees per frame the attacker may still turn during startup.
- `moveSpeed`, `moveUntil`: how much of the walk speed the player keeps during the attack (0, the
  default, roots them), and the frame from which they are rooted. Momentum carries into the swing.
- `armor`: extra poise while the attack runs.
- `bufferFrom`, `chainFrom`, `dodgeCancelFrom`, `next`: the combo and its cancel windows.
- `swing`: how the weapon moves (yaw, pitch, reach). On an enemy it bends the whole body.

### Enemy scripts

An enemy names its script (`"script": "brute"`) and gives it `params`. The script defines
`fun think(me)`, called every frame the enemy is free to act. The game fills `me` with what the enemy
sees; the script answers by setting `me.move`, `me.attack`, `me.strafe`, `me.speed` and `me.face`.
Anything else the script stores on `me` is kept for the next call. The full list is at the top of
`mods/base/scripts/brute.pg`.

Scripts are sandboxed: they get `math`, `algorithm`, `string`, `random` and `print`, and nothing
that reaches the disk, the network or the engine. 200 brutes cost about 0.3 ms per step in total
(about 1 µs per `think` call); `test_voxelarena` measures it.

### Errors

Nothing a mod gets wrong crashes the game. A broken file is reported with its file and line, and the
broken definition is left out:

```
mods/testmod/weapons/spear.json:18: ',' or '}' was expected
mods/base/weapons/sword.json:22: unknown key "startpu" in attack light1 (known: startup, ...)
mods/base/scripts/brute.pg:61: does not compile: Parser Error: Expected expression at line 61, column 5
```

The first error shows at the top of the screen; `errors` and `warnings` in the console list them all.
A script that fails at run time leaves its enemy standing still and shows the message in the overlay.

## Console

Open with `` ` ``. `help` lists the commands.

| Command | What it does |
|---|---|
| `reload` | Reload all content (same as F5) |
| `tune <key> [value]` | Read or set a tuning value until the next reload (`tune player.walkSpeed 7`) |
| `tunes [prefix]` | List tuning values |
| `spawn <enemy> [count]` | Spawn enemies in front of you (`spawn brute 3`) |
| `clear`, `reset` | Remove the enemies; restart the arena |
| `weapon <id>`, `weapons` | Wield a weapon; list them |
| `view [first|third]` | Switch the camera |
| `god`, `ai`, `hitboxes`, `overlay` | Toggles |
| `pause`, `step [n]`, `slowmo <x>` | Freeze, advance n frames, change the speed (frame data checks) |
| `place x y z <block>`, `fill x0 y0 z0 x1 y1 z1 <block>` | Edit the arena (`air` digs) |
| `mods`, `errors`, `warnings` | What is loaded, what went wrong |

Scripted input, for tests and for checking frame data: `mouse off` (ignore the real mouse), `press <action>`, `hold <action> <frames>`,
`wait <frames>`, `sleep <seconds>`, `look <yaw> <pitch>`, `teleport x y z`, `where`, `echo <text>`.
For example:

```sh
./release/VoxelArena --exec "hitboxes; slowmo 0.25; teleport 37.5 4 24.4; wait 30; press light"
```

## Not done yet

- Limb animation is procedural (no keyframes yet).
- The engine plays at most 6 sounds at once: a busy fight skips some. `AudioSystem::setNumberOfChannel`
  resizes its channel array the wrong way round, so the game does not raise it.
- The HUD is laid out for the starting window size and does not follow a resize.
- Chunks, worldgen, lighting and inventory are out of scope for now.
- Falling pieces do not tumble.
