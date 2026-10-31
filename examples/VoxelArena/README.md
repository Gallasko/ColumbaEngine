# Voxel Arena 0.1

A small voxel arena game, the combat demo of the voxel RPG: twenty timed waves of enemies that keep
coming, the shards they drop to spend in a shop between waves, bosses midway and at the end, a score
to beat. Three weapons with their combos and skills, spells you make
yourself, movement tech, an arena you can break, and co-op. A second mod, `testmod`, adds a spear
without any C++.

All the game content lives in `mods/`. The game's own content is the `base` mod, written exactly like
any other mod. Every gameplay number is in a data file and reloads with F5.

## Build and run

```sh
cd release
make -j18 VoxelArena VoxelArenaServer test_voxelarena
cd ..
./release/VoxelArena            # or ./make_and_play.sh VoxelArena
./release/test_voxelarena       # 135 tests, about half a second
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
| `--host [port]` | Host a game others can join (port 7777 by default) |
| `--join <address>[:port]` | Join a game |
| `--name <name>` | Your name, as the others see it |
| `--no-upnp` | Host without asking the router to open the ports |

## The arena run

The title screen: **Enter** for the arena, **T** for the training ground (the sandbox, with dummies
and enemies that come back), **B** for the spellbook, **O** for the options.

- **Its own arena**: a 40 x 40 pit with four pillars, a platform and a boulder on a rope over the
  middle. It is built anew before each wave, whatever was broken, with everyone put back in place.

- **Twenty timed waves**, from a minute to a minute and a half. Enemies keep coming in from the edges
  all through a wave (their spawn point glows red a moment first; one out of view, an arrow at the
  edge of the screen points the way), more and faster as the run goes on, and more with more players.
  Survive until the time is up; whoever is left then goes.
- **Slimes first**: the first waves are slimes only. They hop their way to you (a hop that lands on
  you stings a little), and close up they squash down, glow, pounce, and bounce back off. Skulkers
  join from wave 3, archers from 4, brutes from 6. From wave 7
  some come as **elites**, glowing gold: tougher, harder hitting, three times the shards.
- **Shards**: what the enemies drop. They fly to you within pickup range (and the ones left lying
  are yours at the end of the wave). They are money and XP at once: each **level** gives a free stat
  item to pick in the shop.
- **The weapon choice**: a run begins with each player picking a weapon (1-5 or a click), for the whole
  run: the bow, the dagger, the healer's staff, the sword (and testmod's spear). The first wave waits for
  everyone. No weapon is for sale after that: the shop sells the skills of the one in hand instead
  (`run.json` `chooseWeapon`; off, the start kit's weapon is the one, and weapons are for sale).
- **The bow** (ranged): arrows where you aim. Light is drawn while you hold it: a tap is a weak quick
  arrow, a full draw (0.7 s, the pips under the crosshair) about 2.5 times as hard and faster, and it
  waits for you to let go. Heavy is a slow power shot; Volley
  (five arrows fanned out) and Disengage (a leap back out of reach, an arrow as you go) in the shop.
- **The healer's staff** (support): orbs that burst a little on light, a mending pulse on heavy (health
  back to you and every ally within 5 m); Sanctuary (a circle on the ground healing everyone in it for
  5 s) and Radiance (enemies close by thrown back, allies further out healed) in the shop. In the
  final duel it heals only its holder. An attack heals with `"heal": { "amount", "radius" }`, or leaves
  a healing zone with `"healZone": { "radius", "duration", "perSecond" }`; a projectile shoots
  several at once with `"count"` and `"spread"` (degrees).
- **The shop**, between waves, each player their own: four offers to buy with 1-4 or a click, and
  **Lock** (Shift + 1-4) keeps one for the next shop. A level up's free stat items are on 5 and up.
  **R** rerolls (dearer each time), **Enter** is ready; in co-op the next wave waits for everyone. On sale:
  - stat items (`upgrades.json`): health, damage, armor, crit, speed, regeneration, pickup range,
    dash charges, cheaper spells, faster cooldowns, life on hit... They add up through the run;
  - the skills of your weapon (the weapon itself is chosen at the start, see above),
    spell elements (fire to begin with) and keywords (bolt to begin with). A spell casts only when its
    element and every keyword are bought: the spellbook greys out the rest, the HUD marks a spell
    `(locked)`. With nothing on the keys it can cast, key 1 gets Fire > Bolt for the run.
  Prices go up each wave.
- **Bosses**: the **Brute Chieftain** on wave 10 (a brute twice the size: a slam, a stomp all around
  when crowded, a rush when you keep away) and the **Stone Warden** on wave 20 (a slam, a sweep all
  around, a long charge, a thrown rock; volt cracks it, fire barely warms it, under half its health it
  gets angry). A boss wave lasts until the boss falls, with fewer others around it.
- **Its own arena**: a 40 x 40 pit with four pillars, a platform and a boulder on a rope over the
  middle. It is built anew before each wave, whatever was broken, with everyone put back in place.
- **The final duel**, in multiplayer: after the last wave, instead of the victory at once, everyone for
  themselves. The arena is built anew, everyone is back up at full in a ring around its middle, facing
  it; FINAL DUEL counts down 3 s (no one can hurt anyone yet), then FIGHT. Every hit, spell, arrow and
  zone of a player now lands on the other players too. One life each: the last one standing is the
  champion (CHAMPION on their screen, "<name> won the final duel" on the others'); after two minutes,
  the one with the most of their health left. The `duel` section of `run.json` (`enabled`, `duration`,
  `countdown`, `radius`). Alone, the last wave is the victory as before.
- **The score**: points per kill (twice for an elite), per wave survived, and a bonus for the victory.
  The best is kept (`~/.local/share/voxelarena/best.txt`). **Enter** at the end plays again, **T** goes
  training.
- **Falling**: alone, the run is over. In co-op you are back up at the next wave; the run is lost
  only when everyone is down.
- In multiplayer the host starts the run from its title; on a dedicated server, any player's
  Enter does.

The run is data: `mods/base/run.json` (the waves' durations, spawn rates and pools, elites, bosses,
XP, the start kit, the shop and what it unlocks, the score, its arena) and `mods/base/upgrades.json`
(the stat items and their prices). Enemies take damage by element (`"weakness": { "volt": 1.5 }`),
give `score` and drop `drops` shards. In a run every enemy's script gets `me.hunting = true`: it comes
for the players from anywhere, whatever its engage range.

## Controls

| Key | Action |
|---|---|
| WASD | Move (relative to the camera) |
| Mouse | Turn the camera; wheel zooms |
| Space | Jump (hold for a higher jump); again in the air: double jump, or jump off a wall you touch |
| Left Shift | Dodge roll; keep holding to sprint out of the roll. In the air: air dash (about to land: the landing roll) |
| Left Ctrl / C (held, in the air) | Fast fall |
| Left mouse / J | Light attack (3-hit combo); while sprinting: dash attack; in the air: air slash |
| Right mouse / K | Heavy attack; in the air: plunge |
| Tab | Next weapon (dagger, sword, spear from testmod) |
| F / V | First person (the default) / third person |
| Q | Raise a wall of earth in front of you |
| E | Dig a pit where the crosshair is |
| G / middle mouse (hold) | Mine the block under the crosshair |
| 1, 2, 3 | Your spells, at the crosshair (firebolt, ice shards, chain lightning to begin with) |
| B | The spellbook: build your spells |
| O | Options: mouse sensitivity, invert Y, field of view, volume, camera shake, hit flash, slash trails, view, debug overlay (kept in `~/.local/share/voxelarena/options.json`); the arrows pick and change a row |
| R, 4 | The weapon's skills (dagger: shadow dash, hold to charge, and backflip; sword: whirlwind and leap slam) |
| F1 | Debug overlay |
| F2 | Hitboxes (red: active, orange: winding up) and hurtboxes (green, blue while invulnerable) |
| F5 | Reload all content |
| `` ` `` | Console |
| Esc | Pause and open the options (alone; a shared game goes on); Esc again resumes. Hold Left Alt to free the mouse for a moment |

Bindings are data: the `controls` section of `mods/base/tuning.json`.

## How a fight works

- **Stamina** pays for attacks, skills and sprinting. It refills after a short delay. Hitting zero
  makes you *exhausted*: no sprint, slower refill.
- **Dash charges** pay for the roll and the air dash (and the landing roll, and rolling out of a
  hit): 2 of them, the pips by the stamina bar, one back every 1.2 s (the `dash` section of the
  tuning). Rolling never costs stamina.
- **The dodge roll** covers about 4.5 m and has invulnerable frames (16 of its 27 frames by default).
  You can roll through enemies while invulnerable.
- **Movement help**, all in the `player` section of the tuning:
  - *Ledge assist*: a jump that ends with the feet just under a ledge (`mantleHeight`) climbs onto it.
  - *Corner correction*: a head that clips a block edge on the way up slides past it (`cornerCorrection`).
  - *Momentum*: the takeoff speed is kept in the air and for a moment after landing (`landingGrace`), so
    sprint jumps and roll jumps chain; extra speed then bleeds off quickly (`overspeedDecel`).
  - A jump cancels the roll from `dodge.jumpFrom` on (any time by default) and takes off with the
    roll's speed at that moment: early in the roll is fast and long, but the roll's invulnerability
    ends with it.
- **Landings**: a fall faster than `hardLandingSpeed` slows you for `hardLandingFrames` (to
  `hardLandingMoveSpeed` of the walk speed; 0 makes it a root). A dodge pressed in the air up to
  `landingRollWindow` seconds before landing rolls on touchdown instead. Knocked into the air by a
  hit, the same dodge gets you up at once (wake-up roll).
- **In the air** (the `air` section of the tuning):
  - *Double jump*: `jumps` more jumps before landing. It stops the fall and turns the speed you have to
    where you steer.
  - *Wall jump*: jump while touching a wall to be pushed away from it and up. The double jump comes
    back, so you can climb between two walls (a raised earth wall counts).
  - *Air dash*: Shift in the air, a straight burst with no fall, `dashes` per time in the air. About
    to land (within `landingRollWindow`, or under `dashMinHeight`), Shift is the landing roll instead.
  - *Fast fall*: crouch held in the air. A fast landing can be a hard one: roll it.
- **The camera under a roof** comes down rather than in: when a ceiling blocks it, it lowers until it
  sees the player from its full distance, then eases back up. A wall behind you still pulls it in.
- **First person** (the default; F or V switches to third person and back): the body faces where you look, so you strafe instead of turning, and attacks
  go where you look (no aim assist; the swing follows the view while it runs). The weapon is drawn in front of the eye with the same swing data; the view
  dips during a roll. Its numbers are the `firstPerson` section of the tuning.
- **Attacks** have startup, active and recovery frames. Hitboxes exist only during active frames.
  A light press during an attack is remembered and starts the next attack of the combo once the
  attack allows it (`chainFrom`). A dodge can cancel an attack from `dodgeCancelFrom` on. A light
  press at the end of a roll becomes the dodge attack.
- **The dagger** is fast and keeps you moving (its jabs keep the whole walk speed). Two combos, each
  ending in a move:
  - light, light, light, then light: the *dash finisher*, a long lunge you cannot be hit in, straight
    through the enemy;
  - light, light, light (or heavy, heavy: a spin, then a rising cut that launches), then heavy: the
    *backflip finisher*, a launching cut that flips you up and away. Light after the rising cut is
    the dash.
  Sprint and light is a sliding cut; in the air, light spins a flurry and heavy dives into a stab.
- **Attacks on the move**: a light press while sprinting (or just after) is the *dash attack*, a long
  lunging thrust. In the air, light is the *air slash* (it catches your fall for a moment) and heavy
  is the *plunge*: a short hang, a dive, and a strike all around where you land.
- **Spells** are yours to make: an element, then keywords. `Fire > Bolt > Zone` is a bolt that
  leaves a burning zone where it lands; `Fire > Bolt > Multiply > Zone > Extend` is two bolts, each
  leaving a longer zone.
  - *Forms* are the shapes a spell takes: *bolt* (a shot), *zone* (an area on the ground that hurts
    whoever stands in it), *nova* (a burst all around), *beam* (an instant strike along the aim). The
    first form leaves from you; each one after comes out of the one before it: where a bolt lands,
    where a beam strikes, where a nova bursts, where a zone runs out.
  - *Modifiers* always change the last form before them: *multiply* (two of it), *extend* (it lasts
    or reaches further), *enlarge* (bigger), *chain* (its hit jumps on to two more enemies; bolts and
    beams), *empower* (harder).
  - *The element* decides what it leaves on whoever it hits and what it does to the arena. Fire
    burns, and sets planks and rope alight: the fire spreads along them, and what burns is gone (the
    boulder trap hangs from a rope). Ice chills (slower, stacking; four stacks freeze solid), and
    freezes a dug pit over, a floor to cross until it melts. Volt shocks (a short interrupt that an
    armored attack shrugs off), and breaks stone.
  - *Mana* (the blue bar) pays for them, from the recipe: each keyword adds to the cost, a modifier
    can multiply its form's, and a form is paid once per copy of it (two bolts that each leave a
    zone pay for two zones). Casting never stops a roll.
- **The spellbook** (B) builds them: pick a key (1, 2, 3), then click an element, forms and
  modifiers, or press their keys (elements Z X C, forms A S D F, modifiers Q W E R T: the
  `spellElement1`... `spellModifier5` controls). What cannot follow is greyed out with the reason; the
  recipe shows as a row of blocks with a diagram of what it does, and its cost, cooldown and damage
  update as you build (the part under the mouse previews what it would do). Backspace takes the last
  part off, Delete clears. Save
  it to your book with a name, or pick a ready-made spell. The book is kept in
  `~/.local/share/voxelarena/spellbook.json`. Playing alone, the game waits while it is open.
- **Weapon skills** come with the weapon in hand, on R and 4, and cost stamina and a cooldown (no
  mana), from a standstill or out of the recovery of a basic attack (a combo can end in one). Dagger: *shadow dash*, held to charge (up to 0.75 s: further and up to 2.5 times the
  damage), released to dash through; *backflip*, a kick that flips you up and away. Sword:
  *whirlwind*, two turns hitting all around, still walking; *leap slam*, a leap forward that comes
  down striking around the landing. The cooldown is the slot's: changing weapons does not reset it.
- **Reading attacks**: a swing leaves a fading trail of the blade while it can hit, and an enemy's
  heavy attack (the brute's slam) glows red as it winds up, with a marker over its head and a red
  zone on the ground where it will land, filling in until it strikes.
- **Statuses** (`mods/base/statuses.json`) are what a hit leaves behind: `burn`, `chill`, `frozen`,
  `shock`. Each is data (duration, damage over time, slow, stacks, what it turns into, an
  interrupt), and any attack or projectile can apply one with `"status"`.
- **Poise**: every hit lowers the target's poise. At zero the target is staggered (it cannot act),
  then its poise refills. An attack in progress can add armor, so the brute's slam is not
  interrupted by a jab.
- **Hit feedback**: hitstop (both fighters freeze for a few frames), knockback, a white flash on the
  target, camera shake; hit yourself, the screen flashes red for a moment.
- **The brute** tracks you while it winds up its slam (it glows orange), then commits. Roll late,
  then punish its long recovery. A heavy attack or a full light combo breaks its poise.

## The enemies

Each one is a JSON file in `mods/base/enemies/` and a pgscript brain in `mods/base/scripts/`.

| Enemy | Role | How it fights |
|---|---|---|
| Brute | Tank | Slow, tough, one big slam with a long wind-up. Smashes walls in its way. |
| Archer | Ranged | Keeps 6 to 14 m away, backs off when you close in, moves sideways to find a line of fire when a block is in the way, shoots sooner while you are busy. |
| Skulker | Flanker | Fast and weak. Goes round to your back while someone else holds your attention, bites when you cannot see it, backs off after each bite. |
| Stone Warden | Boss | Slow and huge. Slams, sweeps all around once angry, charges from the middle distance, throws rocks when you keep away. Weak to volt, resists fire. |

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
  10 m and silent past 32 m. Up to 24 play at once.
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

## Multiplayer

2 to 8 players, together against the arena's enemies.

```sh
./release/VoxelArena --host                       # play and host (a listen server)
./release/VoxelArena --join 192.168.1.20 --name bob
./release/VoxelArenaServer --port 7777            # or a dedicated server: no window, no player of its own
```

A game uses two ports: the TCP port given (7777) and the next one for UDP (7778). Open both on the
host's firewall. Players who join need nothing open: the server learns their address from what they send.

**The lobby** is multiplayer's title screen: who is in (by name, the host first), who is ready, their
ping, and the latest joins, leaves and refusals. The host starts the run (Enter) whenever it likes,
"2 of 3 ready" beside the button; the others press Enter to be ready. On a dedicated server there is
no host: the run starts when everyone is ready. T goes to the training ground meanwhile, together;
L brings the lobby back outside a run, and it comes back by itself for those who joined when the host
leaves the end of a run.

**Inviting**: the + at the top left of the title and the lobby (or I). Playing alone, it offers to
host the game right there (no restart: the training ground goes on, now open to others), then shows
the addresses to share with a Copy button each (online when the router opened the ports, and on the
same network) and the command a friend starts the game with. Someone who joined sees the address they
joined, to pass on.

**Joining from the game**: the title's J (or the invite panel's "Join a friend's game instead") asks
for the host's address (`address` or `address:port`, 7777 when none; Paste takes what their invite
copied, the address or the whole command) and the name the others see. Both are kept for next time.
This game becomes a client of theirs on the spot, in their lobby; Leave (Stop hosting, for a game that
started hosting from its title) makes it a game alone again. Console: `join <address>`, `leave`.

**Over the internet** the host's router has to let the two ports in. The host shows how friends join:
its address on the local network, and, when its router opened the ports by itself (UPnP), its public
address. A router without UPnP (or with it off) says so, and the lobby says which ports to forward to
which machine. `--no-upnp` (game or dedicated server) leaves the router alone. The ports are closed
again when the game quits; a mapping left by a crash runs out within an hour.

**Everyone must have the same gameplay content.** The server compares a fingerprint of it and
refuses a player whose content differs: the tuning, the arena, the scripts, and blocks, weapons,
enemies and abilities. What only looks is left out, so each player may have their own: models,
`effects.json`, `sounds.json`, block colours, names, swing poses, camera shake, and the camera,
first-person and controls settings (a texture pack or a sound mod on one machine only is fine). For
the same reason the host cannot reload (F5) while others are connected.

How it works:

- **The server decides.** It runs the one real simulation, the same `ArenaWorld` as single player,
  60 steps a second. A host's game is the server, with its own player in it. A dedicated server is the
  same code with no player of its own.
- **Your own player responds at once.** The client sends its inputs and plays them straight away on
  its own copy of the world. When the server's answer comes back (30 times a second), the client
  starts again from it and replays the inputs the server has not seen yet. A small difference is
  smoothed out over a few frames. A big one snaps.
- **Everyone else is drawn 100 ms in the past.** Other players and enemies are drawn between two
  states the server has already sent, so they move smoothly even when messages are late or lost.
- **Hits are judged on what you saw.** Each input says which moment the player was looking at. The
  server keeps the last 40 steps of everyone's position and checks your attack against where they
  were on your screen.
- **A dodge already on its way still counts.** A hit on a remote player waits half that player's round
  trip. If a dodge arrives that started in time, the hit is cancelled.
- The arena's changes (broken blocks, walls, pits, falling pieces) go to everyone reliably. A player
  who joins late gets the arena as it is now.
- **The server sends gameplay, each machine draws it.** Snapshots carry state (positions, actions
  and their frames, health) and events (a hit on whom, a block broken, an arrow fired from which
  attack). Each client makes the animation, particles, sounds, hit flashes and colours from its own
  content.
- **Effects play when you see them.** Events about your own player (you were hit, you hit something)
  play as they arrive. The rest wait until the others, drawn 100 ms in the past, reach the moment
  they happened, so a spark goes off as the blow is seen landing.

The engine side is `pg::NetworkSystem` (`src/Engine/Networking`), which this work reworked. It carries
reliable messages over TCP and fast ones over UDP, cuts big messages into fragments and puts them
back together, and drops a peer that has been silent for 6 seconds. Its tests run on real sockets
(`t1 --gtest_filter='network*'`). The game's protocol (`Core/net/`) does not depend on it: the
multiplayer tests run a server and clients in one process, over a simulated network with latency,
jitter and lost packets.

On the client, the world-changing console commands (`spawn`, `clear`, `reset`, `place`, `tune`...)
only work on the host. `duel` (host) goes straight to the final duel, and `timeup` ends it. `players` lists who is in the game.

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
    net/         multiplayer: messages, the server, the client (prediction, interpolation),
                 a simulated network for the tests
  Game/          the engine side: input, drawing, HUD, console; the UI (ui.h: an immediate-mode layer
                 of rects, text and pixel icons over the engine's 2D entities, UI_SPEC.md's look)
  Net/           the engine's NetworkSystem as the game's transport
  server_main    the dedicated server
  shader/        the voxel shader
  mods/base/     the game's content
  mods/testmod/  the spear
test/voxelarena/ the tests
```

The UI is drawn in a 1280 x 720 base layout scaled to the window (every number times
`min(width / 1280, height / 720)`, rounded to whole pixels), with two fonts from `res/font` under the
SIL Open Font License: Jersey 15 for numbers, keys and titles, Rubik (static 500, 600 and 700 cuts) for
the rest.

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
| `blocks.json` | blocks: `color`, `solid`, `colorNoise`, `hardness`, `breakable`, `flammable`, `weakness` | `mymod:<key>` |
| `statuses.json` | statuses: `duration`, `damagePerSecond`, `slow`, `maxStacks`, `atMax`, `stagger`, `heavy` | `mymod:<key>` |
| `abilities/<name>.json` | an ability: terrain (`kind`: wall, pit, mine) or a weapon skill (attack) | `mymod:<name>` |
| `elements.json` | the elements of spells: `name`, `status`, `mana` (fire, ice, volt) | `mymod:<key>` |
| `keywords.json` | the keywords of spells: forms (`shape`: bolt, zone, nova, beam) and modifiers | `mymod:<key>` |
| `spells/<name>.json` | a ready-made spell: `name`, `recipe` (`["fire", "bolt", "zone"]`) | `mymod:<name>` |
| `upgrades.json` | the stat items of a run: what each adds (`maxHealth`, `damage`, `armor`, `crit`, `regen`, `pickupRange`, `dashCharges`...), its `price`, how many one can have (`most`) | `mymod:<key>` |
| `run.json` | the arena run: waves, spawn pools, elites, bosses, XP, the start kit, the shop and its unlocks, score, its arena (the last mod with one wins) | |
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
- `element` (`fire`, `ice`, `volt`) and `status` (an id, or `{ "id": ..., "stacks": n }`): what the hit
  carries.
- `gravityScale`: the fall while the attack runs (an air attack hangs). `plungeSpeed`: the attack dives
  after its startup and holds its first active frame until it lands.
- `iframes` `[from, to)`: frames the attacker cannot be hit, passing through whoever is in the way.
  `leap` `[along, up]` m/s at frame `leapAt` (negative along: backwards; enemies leap too, the slime hops with it). `turn` (`pitch`, `yaw`,
  `from`, `to`): how the body turns over the attack, a backflip or a spin; it only looks.
- `dash`: the lunge sets the speed at once instead of speeding up to it. `charge` (`at`, `frames`,
  `lunge`, `damage`): held, the attack waits at frame `at` up to `frames`, then lunges and hits up to
  `lunge` and `damage` times harder. `tell`: `"heavy"` gives an enemy attack the heavy warning (looks only).
- The moveset names `light`, `heavy`, `dodgeAttack`, `dashAttack`, `airLight` and `airHeavy`.

A weapon has a `type` (`dagger`, `sword`...). A weapon skill is an ability with `"kind": "attack"`,
an `attack` like a moveset's, and `weapons`, the types it works with. A slot of `player.abilities` can
be a list, `["base:shadow_dash", "base:whirlwind"]`: the first one the weapon in hand allows is there.

A spell keyword (`keywords.json`, the header comment explains every field) is a form, with its
numbers (`speed`, `radius`, `range`, `duration`, `damage`...), or a modifier, with the multipliers it
applies to its form (`count`, `durationScale`, `radiusScale`...) and the forms it can follow. Both
add `mana` and `cooldown`; a modifier's `manaScale` multiplies its form's cost. A mod adds a keyword
and every player can use it in their spells at once. What spells look like is the client's: a
bolt's `model` by element on its keyword, a zone's tint and particles in `effects.json` (`zone:fire`),
a status's under its id (`base:burn`), and `"along": true` spreads an effect along a line (a beam).

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
| `stamina [on\|off]`, `mana [on\|off]` | Infinite stamina (and dash charges), infinite mana for the players (for testing moves and spells) |
| `pause`, `step [n]`, `slowmo <x>` | Freeze, advance n frames, change the speed (frame data checks) |
| `place x y z <block>`, `fill x0 y0 z0 x1 y1 z1 <block>` | Edit the arena (`air` digs) |
| `mods`, `errors`, `warnings` | What is loaded, what went wrong |
| `players` | Who is in the game, and the round trip to the server |
| `spell <1-3> <recipe>` | Put a spell on a key: `spell 1 fire bolt multiply zone extend`, or a ready-made id: `spell 2 frost_nova` |
| `spells`, `spellbook` | The spells on the keys, the ready-made ones and the keywords; open the spellbook |
| `arena`, `training`, `wave <n>` | Start the arena run, go back to training, jump to a wave of the run |
| `timeup`, `kill`, `shards <n>` | End the wave now (a boss wave: its boss falls), kill every enemy (they drop their shards), get shards (and their XP) |
| `pacer [n\|off [fps]]` | The engine's update pacing: n updates per drawn frame (2 by default), or off (as fast as it can, or capped at fps) |

Scripted input, for tests and for checking frame data: `mouse off` (ignore the real mouse and let go of the cursor), `click <x> <y>` (a click on the
UI, window pixels), `press <action>`, `hold <action> <frames>`,
`wait <frames>`, `sleep <seconds>`, `look <yaw> <pitch>`, `teleport x y z`, `where`, `echo <text>`.
For example:

```sh
./release/VoxelArena --exec "hitboxes; slowmo 0.25; teleport 37.5 4 24.4; wait 30; press light"
```

## Not done yet

- Limb animation is procedural (no keyframes yet).
- The HUD is laid out for the starting window size and does not follow a resize.
- Chunks, worldgen, lighting and inventory are out of scope for now.
- Falling pieces do not tumble.
- Spells cast at once, with no cast animation, and a burning block that falls goes out.
- The spear (testmod) has no dash or air attacks: in the air it uses its ground attacks.
- Competences (passives and skills you combine, mastery points) come later; spells and moves are
  data so a competence can grant them.
- Multiplayer: no reconnect after a lost connection (start the game again), no host migration, and
  the enemies' scripts run only on the server. Two windows on one desktop take the mouse from each
  other: press Esc (the options) in the one you are not playing.
