# Voxel Arena 0.1

A small voxel arena game, the combat demo of the voxel RPG: twenty timed waves of enemies that keep
coming (each arena brings its own, and elites come with affixes that change how they fight), the
shards they drop to spend in a shop between waves, bosses midway and at the end, a score to beat. Three weapons with their combos and skills, spells you make
yourself, movement tech, an arena you can break, and co-op. A second mod, `testmod`, adds a spear
with its levels, branches and techniques without any C++.

All the game content lives in `mods/`. The game's own content is the `base` mod, written exactly like
any other mod. Every gameplay number is in a data file and reloads with F5.

## Build and run

```sh
cd release
make -j18 VoxelArena VoxelArenaServer test_voxelarena
cd ..
./release/VoxelArena            # or ./make_and_play.sh VoxelArena
./release/test_voxelarena       # 277 tests, about eight seconds
# Each weapon's damage a second, at level 1 and level 5 with each branch, on one dummy and on three:
./release/test_voxelarena --gtest_also_run_disabled_tests --gtest_filter='*weapon_dps*'
```

The game reads its mods and its shader straight from the source tree (`examples/VoxelArena/mods`,
`examples/VoxelArena/shader`), so editing a file and pressing F5 needs no rebuild. It looks for the
engine's `res/` and `shader/` in the working directory, and moves to the repository root by itself
when they are not there. A copy of the game away from the source tree (a player's) reads `mods/`
and `arena-shader/` next to the program, and moves there when started from another folder.

What the game saves goes in `~/.local/share/voxelarena` (`$XDG_DATA_HOME/voxelarena` when it is
set; `%APPDATA%\VoxelArena` on Windows): the options, the spellbook, the levels won, the best
score, and the player's own mods (see Making a mod).

Options:

| Option | What it does |
|---|---|
| `--mods <dir>` | Load the game's mods from another folder (the player's own still load) |
| `--exec "<commands>"` | Run console commands at start, joined with `;` (see Console) |
| `--host [port]` | Host a game others can join (port 7777 by default) |
| `--join <address>[:port]` | Join a game |
| `--name <name>` | Your name, as the others see it |
| `--no-upnp` | Host without asking the router to open the ports |
| `--lab [script.pg]` | Start in the lab (see The lab), with its scenes from `tools/lab/lab.pg` or the script given |

## The arena run

The title screen: **Enter** for the arena, **T** for the training ground (the sandbox, with dummies
and enemies that come back), **O** for the options.

- **The difficulty**, chosen as a run starts (and again with Play again): a card for each level of
  `run.json`'s `difficulties` (Normal, Hard, Very hard, Nightmare, Torment, Hell), with its waves and what it
  changes. 1-6, a click, or the arrows and Enter play one; Backspace goes back. The first is open, and
  winning a level opens the next (`~/.local/share/voxelarena/levels.txt`); the locked ones show what opens
  them. In multiplayer the host chooses (the others see who is choosing); on a dedicated server, the first
  player in, with every level open. Console: `arena` goes straight to wave 1 at the last level picked,
  `arena choose` shows the choice.

- **Five arenas**, four waves each, every one built around the terrain (`mods/base/arenas/`):
  - *The Pit* (waves 1-4): four pillars, a platform, a boulder on a rope over the middle.
  - *The Stockade* (5-8): a timber yard. The barn's brick roof stands on eight plank posts: burn
    them all and it comes down on whoever is under it. Watchtowers with a boulder on each deck, hay
    that catches at a spark.
  - *The Gorge* (9-12): two plateaus and a stepped ravine, three plank bridges with rope rails (fire
    runs along the rope and drops the middle of the bridge into the ravine), two boulders hanging
    over it.
  - *The Icefall* (13-16): snow, a frozen pond whose crust breaks (an enemy that falls in cannot
    climb out; you can jump), ice frost caps on thin stems, stone arches hung with icicles (volt
    both legs).
  - *The Quarry* (17-20): a stone mesa with stairs (volt breaks stone four times as fast), two
    plank cranes with stone loads on ropes, stone tables on one plank prop.
  - *The Crucible*: the final duel's. An island ringed by planks over a trench, a boulder over the
    dais, a brick-roofed pavilion on each diagonal.

  A new arena is built as its first wave starts (its name under the wave's), everyone at its spawn
  point; within an arena it is built anew before each wave, whatever was broken. The HUD names it
  under the wave's pips. Enemies walk up one block; higher, up to four, they climb (slowly: see Terrain as a
  weapon), so every floor still has steps.

- **Twenty timed waves**, from 45 seconds to a minute and a half. Enemies keep coming in from the edges
  all through a wave (their spawn point glows red a moment first; one out of view, an arrow at the
  edge of the screen points the way), more and faster as the run goes on (`spawnScale` 0.75 of the
  written rates for one player), and half again for each player past the first. Survive until the
  time is up; whoever is left then goes.
- **Slimes first**: the first waves are slimes only. They hop their way to you (a hop that lands on
  you stings a little), and close up they squash down, glow, pounce, and bounce back off. Skulkers
  join from wave 3, archers from 4, brutes from 6.
- **Each arena brings its own enemy**, from its first wave on (rarer after it): the Stockade's
  **sappers** (from wave 5), the Gorge's **menders** (9), the Icefall's **rimecallers** (13) and the
  Quarry's **golems** (17). See The enemies below.
- **Elites**: from wave 7 some come as elites: tougher, harder hitting, three times the shards, and now
  and then (35%, three a wave at most) a **present**: a gold box with a red ribbon. Each one picked up is
  a free shop item, picked among three after the level ups. An elite comes with an **affix**
  (`affixes.json`), two from wave 15 (`run.json` `eliteAffixes`). It glows its affix's colour (two: from one to the other) and
  their names show over its bar:
  - *Blazing*: leaves fire behind it as it walks, and its hits burn;
  - *Frosty*: its hits slow you (frost: 30% slower, it never freezes you solid), and it bursts into frost as it dies;
  - *Guardian*: enemies within 6 m of it take half damage (they glow white: kill it first);
  - *Volatile*: explodes a second after it dies (a red ring on the ground shows where);
  - *Swift*: 45% faster;
  - *Splitting*: splits into two plain copies with 40% of the health as it dies;
  - *Regenerating*: heals 6% of its health a second once you stop hitting it for 2 s.
- **Blasts**: what explodes as it dies (a sapper, a volatile elite) first shows a red ring on the ground
  that fills in, for its `delay`, while its body counts down (it swells and blinks faster and faster, sparks
  fly off its fuse: a blast's `"fuse"` is where on the model); then everyone in it is hit, a shockwave
  sweeps the ground, and nothing is left of the body. Enemies' ground (a blazing trail, a frost
  puddle) has a red edge: it hurts you, not them.
- **Shards**: what the enemies drop. They fly to you within pickup range (and the ones left lying
  are yours at the end of the wave). They are money and XP at once. In co-op every player gets every
  shard anyone picks up, money and XP.
- **Spells and techniques come from levels**: no spell is made in a run. After the weapon, each
  player picks a **first spell** for key 1 (1-3 or a click; Backspace back to the weapons) among three
  drawn at random each run, of different elements while it can (Firebolt, Frostbolt, Spark, Fireball,
  Chain lightning...) — `run.json` `start.chooseSpell` (the pool) and `chooseCount` (how many). As a wave ends, after a moment (**Wave cleared**: 2.5 s in which nothing can be clicked, so a
  click from the fight buys nothing), each level up offers three picks (1-3 or a click): a **new
  spell** (while one of the three spell keys is free), a **new technique** of the weapon in hand, the
  **next level of the weapon** in hand (it starts at level 1), or the **next level** of one you have.
  One pick is always a next level of something you have, the other two are new, until every spell key
  and technique is filled; after that all three are next levels. Each player has 3 **rerolls** of the
  picks for the whole run (R, or the Reroll button) and 3 **banishes** (Shift + 1-3, or a card's Banish
  button): a banished spell or technique is never offered again in the run, and another pick of the
  same kind takes its place. A branch choice cannot be banished (`run.json` `levelRerolls`,
  `levelBanishes`). Levels never run out:
  - a spell's written `levels` add keywords (Firebolt II adds Enlarge, III Extend, IV Empower); a
    technique's add a boost (+15% damage, a faster recharge...); the weapon's boost its light and
    heavy attacks (Iron Sword II: +15% damage, III: +10% attack speed...);
  - at level 5 (`run.json` `branchLevel`) it **branches**: its branches are offered side by side and
    you take one, for good. Firebolt's Inferno makes its burning patch burst as it ends, Twin bolts
    shoots one more bolt; the Whirlwind's Firestorm sets enemies on fire, Cyclone spins far wider; the
    sword's Flame blade sets enemies on fire, Cleave swings far wider...
    A branch adds keywords (a spell), an element and a status (a technique, a weapon), tags and a boost.
    It shows: a weapon whose branch gives it an element glows it and sheds it from the blade
    (`effects.json` `weapon:fire`...), and a slash's trail takes the attack's element and grows with its area;
  - every level past those is **mastery**: +10% damage and a 3% faster recharge each (`mastery`); a weapon,
    +10% damage and 3% more attack speed (`weaponMastery`).

  Each card shows what changes, before -> after (green better, red worse). The XP to the next level
  grows slowly (10, then 3 more each level). Then the presents, then the shop.
- **The weapon choice**: a run begins with each player picking a weapon (1-5 or a click; the melee ones,
  sword, dagger and spear, mend 0.5 health a second while in hand: a weapon's `regen`), for the whole
  run: the bow, the dagger, the healer's staff, the sword (and testmod's spear). The first wave waits for
  everyone. No weapon is for sale after that: level ups offer the skills of the one in hand instead
  (`run.json` `chooseWeapon`; off, the start kit's weapon is the one, and weapons are for sale).
- **The bow** (ranged): arrows where you aim. Light is drawn while you hold it: a tap is a weak quick
  arrow, a full draw (0.6 s, the pips under the crosshair) three and a half times as hard and faster, and it
  waits for you to let go. Heavy is a slow power shot; Volley
  (five arrows fanned out) and Disengage (a leap back out of reach, an arrow as you go) by level ups.
- **The scythe** (melee): slow, wide sweeps with a long reach that hit whole groups. Light is two
  sweeps then a spin all around you; heavy is the reap, a slow cut that pulls enemies towards you (a
  negative `knockback` pulls). It gives less mana back per point of damage (`"mana": 0.2`), as it hits
  many at once. Harvest (a spin that also heals you a little) and Grim vault (a flip forward over
  enemies, out of reach, landing with a sweep) by level ups.
- **The spear** (testmod, melee): quick thrusts with the longest reach, a wide sweep as the heavy. Its
  levels make it hit harder and reach further; its branches are Impaler (much longer thrusts) and Storm
  spear (hits stun). Javelin (a spear thrown far and hard) and Pole vault (a vault over enemies, coming
  down point first) by level ups. All of it is data in the mod.
- **The healer's staff** (support): orbs that burst a little on light, a mending pulse on heavy (health
  back to you and every ally within 5 m); Sanctuary (a circle on the ground healing everyone in it for
  5 s) and Radiance (enemies close by thrown back, allies further out healed) by level ups. In the
  final duel it heals only its holder. An attack heals with `"heal": { "amount", "radius" }`, or leaves
  a healing zone with `"healZone": { "radius", "duration", "perSecond" }`; a projectile shoots
  several at once with `"count"` and `"spread"` (degrees).
- **The shop**, after the level ups and the presents, each player their own: four offers to buy with 1-4 or a click,
  and **Lock** (Shift + 1-4) keeps one for the next shop. **R** rerolls (dearer each time), **Enter**
  is ready; in co-op the next wave waits for everyone. On sale, only items (`upgrades.json`):
  - stat items: health, damage, armor, crit, speed, attack speed (Frenzy: the weapon's light and heavy
    attacks 10% quicker each, and as much cheaper in stamina), regeneration, pickup range, dash charges,
    cheaper spells, cheaper attacks and techniques in stamina (Tireless), faster cooldowns, life on hit...;
  - tag items: a boost for **every** spell and technique of yours with a tag. Spells are tagged with
    "spell", their element and their shapes ("projectile" for a bolt, "area" for a nova or a zone,
    "zone", "beam"); techniques with "technique", their own ("melee", "area", "dash", "heal") and
    "projectile" when they shoot; your weapon's light and heavy attacks with "attack", its role
    ("melee", "ranged", "support"), its type and "projectile" when they shoot (they take damage, area
    and projectile speed, and Split shot shoots one more arrow or orb). Searing: +20% fire damage; Honed: +20% melee damage (your
    sword's swings too); Whetstone: +15% weapon attack damage; Split shot: +1 projectile; Fleet: +25%
    projectile speed; Widening: +20% area size; Lingering: +30% zone duration; Hastened: -15% spell
    cooldown... One is offered only when something of yours has
    its tag. Leech (life on hit) heals in full from a direct hit (a
    swing, an arrow, a bolt or a beam striking), a fifth from an area's, a zone's or a chain's.
  - trade-offs, something big for something lost: Glass cannon (+35% damage, -30% max health),
    Juggernaut (+12% armor and 20 health, 12% slower), Cursed coin (25% more shards, you take 20% more
    damage), Overclock (25% shorter cooldowns, spells cost 35% more and stamina comes back 30% slower).
    A rarity only makes the good side bigger: the downside stays as written;
  - items for the moment: Last stand (+40% damage under half health), Sky striker (+35% in the air),
    Executioner (+50% to enemies under 30% health), Grudge (+40% for 3 s after you take a hit);
  - triggers: Thorns (enemies that hit you take 80% of it back), Soul siphon (heal 2 on each kill), Ember
    trail (a roll leaves burning ground where it starts), Static charge (every 5th weapon hit deals 14
    lightning damage and jumps to 2 more enemies). Their damage grows with the waves, as the enemies' health does;
  - payoff items, for statuses and combos, offered only when something of yours makes what they need
    (`needs`, `needsElements`): Catalyst (combos do 40% more damage; with two elements), Kindling
    (+30% damage to burning enemies), Brittle (+30% to slowed and frozen ones), Pyre (burning enemies
    you kill explode for 14 damage and set others on fire, the burn's own kills too: a status
    remembers who left it), Long fuse (your statuses last 50% longer).

  Hover a card: a tooltip lists what buying it changes, before -> after (your numbers, each spell's
  and technique's). Each item comes at a **rarity**: Common, Rare (x1.5 its numbers), Epic (x2.1),
  Legendary (x3), shown by the card's colour, dearer the rarer; rarer ones come up more as the run
  goes on (`run.json` `shop.rarities`). They add up through the run, with no limit: an item stops
  coming only once one more would do nothing (armor at 70%, cooldowns and spell costs 70% off, crit at
  100%), or for the few with a `most` (dash charges, status stacks, speed, attack speed, projectiles,
  horde). Some come at one rarity only (`tiers`): Split shot, Nimble and Potent, whose +1 a rarity does not
  make bigger, are always Rare. **Prices** start at half and end at one and a half times the written ones (`shop.priceScale`). In co-op
  they are half again for each player past the first (`shop.coopPrice`), the reroll too: everyone gets every shard. **Interest**: the shards you keep
  through a shop grow by the next one as much as the prices did, so what you could buy and did not,
  you still can (shown under your shards). **Greed** (each shard of a kill may come with one more) adds
  up across the players with diminishing returns (`run.json` `shardFalloff`: a total of +100% counts as
  +67%).

  **Items show on you**: each one you have is worn (shoulder pads for Tough, a breastplate for Plated,
  horns for Horde, a coin pouch for Greed, wings at the heels for Nimble...), and each tag item is a gem
  circling you, in its colour and the shape of what it is about (a flame for Searing, a clock for
  Hastened, a skull for Executioner), in rings of eight that turn either way. The rarer, the more it
  glows: Rare a blue glow, Epic a purple pulse and sparkles, Legendary a gold pulse and more sparkles
  (`effects.json` `rarity:rare`, `rarity:epic`, `rarity:legendary`). The others see yours. They are
  data: an item's `look` in `upgrades.json` (a model on a body part, or `"orbit"`: a gem's colour, with
  `"icon"`: one of the UI's pixel icons); the base mod's pieces come from `tools/make_looks.py`.
  Console: `item <id|all> [rarity]` gives you items.
- **Bosses**: the **Brute Chieftain** on wave 10 (a brute twice the size: a slam, a stomp all around
  when crowded, a rush when you keep away) and the **Stone Warden** on wave 20 (a slam, a sweep all
  around, a long charge, a thrown rock; volt cracks it, fire barely warms it). A boss wave lasts until
  the boss falls, with fewer others around it. Each boss has **three phases**. At two thirds and at one
  third of its health it shields itself (no hit takes it past that line), and crystals rise around the
  arena: three for the Chieftain, four for the Warden. Arrows at the edge of the screen point to them.
  While the shield is up nothing hurts it. Break every crystal and the shield falls and the boss reels.
  It gets angrier with each phase. A boss has three quarters more health for each player past the first
  (`bossCoopHealth`). A boss's `phases` in its enemy file sets this up.
- **Stacked damage helps less and less**: extra damage from stat items counts in full up to +40%, then
  less and less, never past about +207%. Tag items and mastery together work the same way, on their own
  (`damageFalloff` in run.json). Enemies have 8% more health each wave.
- **The final duel**, in multiplayer: after the last wave, instead of the victory at once, everyone for
  themselves. In the Crucible, everyone is back up at full in a ring around its middle, facing
  it; FINAL DUEL counts down 3 s (no one can hurt anyone yet), then FIGHT. Every hit, spell, arrow and
  zone of a player now lands on the other players too. One life each: the last one standing is the
  champion (CHAMPION on their screen; everyone's end screen names the DUEL WINNER); after two minutes,
  the one with the most of their health left. The `duel` section of `run.json` (`enabled`, `duration`,
  `countdown`, `radius`, `arena`). Alone, the last wave is the victory as before.
- **The score**: points per kill (twice for an elite), per wave survived, and a bonus for the victory.
  The best is kept (`~/.local/share/voxelarena/best.txt`). **Enter** at the end plays again, **T** goes
  training.
- **Your damage**: the end screen's panel on the right lists what your damage came from, the most first:
  your weapon, each spell and technique, element combos, burns, item triggers, the sapper blasts you set
  off... with each one's share. Alone and in co-op.
- **The damage meter**: a training dummy shows under its bar what it took and how much a second, for a few
  seconds after the last hit (the training ground and the lab).
- **Damage numbers**: your hits' damage rises off what you hit (bigger for a kill, gold for a crit; a
  quick run of hits on one enemy adds up into one number). Options: Damage numbers.
- **The recap**: with two players or more, the end screen has a row for each player: their level,
  kills, damage dealt, healing (when someone healed), shards and XP picked up, presents picked up,
  items bought and falls. Console: `bot` adds an idle player, to try the duel and the recap alone.
- **Falling**: alone, the run is over. In co-op you are back up at the next wave; the run is lost
  only when everyone is down.
- In multiplayer the host starts the run from its title; on a dedicated server, it starts when
  everyone is ready.

The run is data: `mods/base/run.json` (the waves' durations, spawn rates and pools, elites, bosses,
XP, the start kit and spells, the level ups' skills, the branch level and mastery, presents, the pause
after a wave, the shop, its prices and rarities, the score, the arenas by wave), `mods/base/upgrades.json`
(the stat and tag items and their prices), `mods/base/spells/` (each spell's levels and branches),
`mods/base/abilities/` (each technique's tags, levels and branches) and `mods/base/arenas/`. Enemies take damage by element (`"weakness": { "volt": 1.5 }`),
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
| 1, 2, 3 | Your spells, at the crosshair (training: firebolt, ice shards, chain lightning; a run: what level ups gave) |
| O | Options: mouse sensitivity, invert Y, field of view, volume, camera shake, hit flash, slash trails, view, frame rate (30 to 240, or no limit), debug overlay, damage numbers (kept in `~/.local/share/voxelarena/options.json`); the arrows pick and change a row |
| R, 4 | The weapon's skills (dagger: shadow dash, hold to charge, and backflip; sword: whirlwind and leap slam) |
| F1 | Debug overlay |
| F2 | Hitboxes (red: active, orange: winding up) and hurtboxes (green, blue while invulnerable) |
| F3 | The lab (see The lab), in a game alone |
| F5 | Reload all content |
| `` ` `` | Console |
| Esc | Pause and open the options (alone; a shared game goes on); Esc again resumes. Hold Left Alt to free the mouse for a moment |

Bindings are data: the `controls` section of `mods/base/tuning.json`. Players rebind them in the game:
Options, then Controls (moving, jumping, dodging, the attacks, the spells, the weapon skills, the terrain
abilities, the next weapon): click a key, press the new one (a key or a mouse button: left, right,
middle, and the two side buttons, Mouse4 and Mouse5). A key another
action had moves to it the other way round (they swap); Reset to defaults clears them. The keys are kept
in `options.json` (`"keys"`) over the tuning's, and are named as the keyboard prints them (an AZERTY shows
Z Q S D where a QWERTY shows W A S D: the same keys, by position).

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
  - *Air dash*: Shift in the air, a straight burst with no fall, `dashes` per time in the air; out of an air
    attack too, once it has struck (not a plunge). About to land (within `landingRollWindow`, or under
    `dashMinHeight`), Shift is the landing roll instead.
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
    zone pay for two zones). Casting never stops a roll. Your weapon's hits give mana back: 0.3 for
    each point of damage its light and heavy attacks do (`tuning.json` `mana.fromWeapon`, or a
    weapon's own `mana`), so fighting with the weapon pays for the next spell.
- **The spellbook** (console: `spellbook`; a tool for mods and the training ground, not part of a
  run, where level ups give the spells) builds them: pick a key (1, 2, 3), then click an element, forms and
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
- **Element combos** (`mods/base/combos.json`): a status landing on an enemy that already has another
  one sets off a combo, either way round, for everyone from the start:
  - *Melt*: fire meets cold (burn and chill or frozen). The hit again plus 12, and both are used up.
  - *Overload*: lightning meets fire. An explosion 3 m around that sets the enemies in it on fire.
  - *Shatter*: lightning on a frozen enemy. Three times the hit plus 25, and the freeze breaks.

  A combo is one more hit: `damage` times the hit that set it off plus `base` (which grows with the
  waves as the enemies' health does), on the target or on everyone within `radius`. A combo's hit sets
  off no combo of its own, and the same enemy has at most one combo every `cooldown` s. Each has its
  effect in `effects.json` under its id. So a weapon or a technique with an element (Flame blade,
  Frost fang, Storm arrows...) and a spell of another one work together.
- **Payoff branches**: some spell branches cash in on statuses instead of adding numbers. Fireball's
  *Detonate* deals the burn left on what it hits at once, plus 50% (`"detonate": 1.5`; a burn that has
  only just landed is left alone). Frost nova's *Ice breaker* does triple damage to frozen enemies, and
  Chain lightning's *Conductor* +60% to burning, slowed or frozen ones (`"against": { "frozen": 2 }`;
  `"any"` for any status). Frostbolt's *Frost weapon*, Twin embers' *Flame weapon* and Thunderclap's
  *Storm weapon* imbue your weapon each time you cast: for 5 s your weapon hits also slow, burn or stun
  (`"onCast": "frost_weapon"`, a status whose `"imbue"` is the status your weapon hits leave). With
  Flame blade, Frost weapon melts on every swing. Any branch (a technique's, a weapon's) can take
  `against` and `detonate`.
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
| Stone Warden | Boss | Slow and huge. Slams, sweeps all around once angry, charges from the middle distance, throws rocks when you keep away. Weak to volt, resists fire. Three phases, shielded by four crystals between them. |
| Sapper | Bomber | The Stockade on. Runs straight at you; close enough, it lights its fuse: sparks fly off it, and it shakes, swells and blinks red and white, faster and faster, then goes up. Kill it before that and its keg does not go off. Any other end (its fuse burning down, another sapper's blast) and it slumps over its keg and the fuse burns on: the keg explodes a second later (a fireball, a shockwave over the ground, smoke, debris, and nothing left of it). A red ring shows where, and it hurts everyone in it, enemies too, and sets planks and hay alight. |
| Mender | Support | The Gorge on. Keeps behind the others like an archer, and when some of them are hurt it mends everyone within 7 m (a green pulse, stronger as the waves go on). Never mends a boss's crystal. Its own attack is a weak bolt. |
| Rimecaller | Area denial | The Icefall on. Keeps its distance and lobs orbs of frost over walls; where one lands the ground stays frozen for a few seconds and slows you. Fire hurts it more. |
| Golem | Tank | The Quarry on. Slow and tough, slams the ground through blocks. As it falls it breaks into three pebbles. Lightning cracks it. |
| Pebble | Swarm | What a golem breaks into: small and quick, it scurries at you and nips. |
| Shield Crystal | Object | Rises around the arena when a boss shields itself. Never moves or fights. Break them all to drop the shield. |

Scripts work as a group through what the game puts in `me`: `alliesNear`, `alliesAttacking`,
`alliesNearTarget` (other enemies close to you), `alliesHurt` (other enemies within 10 m under 80% of their
health: what the mender mends) and `targetFacing` (0 = in front of you, 180 =
behind). A script answers with `me.move` (`hold`, `chase`, `strafe`, `retreat`, `flank`), `me.attack`,
and for `flank` the distance it keeps, `me.radius`. The full list is at the top of `brute.pg`.

**Projectiles** are part of an attack: a `projectile` object (`speed`, `gravity`, `lead`, `damage`,
`poiseDamage`, `terrainDamage`, `spawn`, `model`...) fires on the attack's first active frame. An
enemy aims at your chest, raised for the arc and leading you by `lead`. A roll's invulnerable frames
let an arrow through; a block stops it (and takes `terrainDamage`), so a raised wall is cover.
`burst` hits everyone that close to where it lands, and `ground` leaves a zone there (`radius`,
`duration`, `perSecond`, `element`, `status`: the rimecaller's frost puddle).

## Sounds, particles and animation

- **Shadows**: a soft dark disc on the ground under every fighter and projectile, wider and fainter the higher
  it is (gone past 10 m): where something is over the ground, and how high, reads at a glance.

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
- An enemy in a hole climbs out of it: walking into the wall of a hole (walls on three sides), or of anything
  with its player above it, it climbs up, from two blocks to four (`climb` in the tuning). A big one climbs
  faster than a small one, and a high wall slows it down: a brute is out of a two-deep pit in a little over a
  second, a pebble out of a four-deep one in about fifteen. A hit knocks a climber off, and a wall on open ground
  (your raised earth) is never climbed. Its script knows (`me.climbable`): the brute climbs out of a pit rather
  than slam its wall (a slam would dig it deeper). Deeper than four blocks, a hole keeps it. A pit fills back
  in after `lifetime` seconds.

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

**Multiplayer, from the title** (M): Host a game (H) makes this game the host right there (no restart:
the training ground goes on, now open to others), then shows the addresses to share with a Copy button
each (online when the router opened the ports, and on the same network) and the command a friend starts
the game with; Join a game (J) asks for a friend's address. In the lobby, the + at the top left (or I)
shows the addresses again; someone who joined sees the address they joined, to pass on.

**Names**: the options' first row is the name the others see (click it, type, Enter); it is kept, the
host's lobby shows it, and a player who joined tells the server when it changes. In a game with others
every player has their name over them (an ally's health under it, yours in third person).

**Back to the title**: To title, in the options while playing (alone, a run's end; hosting, the run ends for
everyone, back to the lobby; a player who joined leaves). In a run it asks for a second click.

**Quitting**: Quit game, at the bottom of the title and the lobby and in the options, asks for a second
click within 3 s (a player who joined says goodbye to the host first). The menus (options, spellbook)
pause the game when nobody else is in it, hosting with nobody joined included.

**Joining from the game**: Join a game (the title's Multiplayer, or J) asks
for the host's address (`address` or `address:port`, 7777 when none; Paste takes what their invite
copied, the address or the whole command) and the name the others see. Both are kept for next time.
This game becomes a client of theirs on the spot, in their lobby; Leave (Stop hosting, for a game that
started hosting from its title) makes it a game alone again. Console: `join <address>`, `leave`.

**Over the internet** the host's router has to let the two ports in. The host shows how friends join:
its address on the local network, and, when its router opened the ports by itself (UPnP), its public
address. A router without UPnP (or with it off) says so, and the lobby says which ports to forward to
which machine. `--no-upnp` (game or dedicated server) leaves the router alone. Some routers only open some outside
ports (a Freebox: above 45000): the game asks for its own first, then 50000, then 45000, 45002... up to 45200 (each
pair, TCP and the next for UDP, led to the game's 7777 and 7778), and the lobby and the invite show the one
the router took (`81.2.3.4:45000`). `--upnp-ports 7777,45000-45200,50000` sets the ports tried, in order. The ports are closed
again when the game quits; a mapping left by a crash runs out within an hour.

**Choosing the port yourself**: when the search finds nothing, type a port in the Multiplayer panel (or `host 5000`
in the console, `host auto` to go back). The game then listens on that port for TCP and the next one for UDP,
asks the router for that pair and no other, and shows the address to share even if the router refused, since
you may have forwarded the two ports by hand. The port is kept for next time.

**Everyone must have the same gameplay content**, and players get it from the server. The server
compares a fingerprint of it (the tuning, the arena, the scripts, and blocks, weapons, enemies and
abilities). A player whose content differs is sent the server's mods (every mod it loaded, from the game's
folder and its player's, none it turned off: `.json`, `.pg` and `.png` files, 8 MB at most) and plays on them until leaving, when their own come back. They are checked before anything is
written, kept in a folder of their own, and their scripts are not run: only a server runs scripts. A
different version of the game cannot be fixed that way: it is refused, and needs the same build.
What only looks is left out of the fingerprint, so each player may have their own: models,
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
- **Snapshots only carry what changed.** Each client tells the server the newest snapshot it read. The
  next ones only carry what changed since then: who came, who left, and the bytes that differ for
  the rest. A late wave's snapshot is about 6 KB instead of 27 KB. A lost one does not matter: the
  next is still built on one the client has.
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
  tools/lab/     the lab's scenes (lab.pg); Game/lab is its panel and script runner
  shader/        the voxel shader
  mods/base/     the game's content
  mods/testmod/  the spear, its levels, branches and techniques (javelin, pole vault): a weapon from a mod, no C++
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

A mod is a folder with a `mod.json`. The game's own mods are in `mods/` next to the program; a
player's go in their own **mods folder**, `~/.local/share/voxelarena/mods` (`%APPDATA%\VoxelArena\mods`
on Windows), which the game makes on its first start. Both load, the game's first: a mod in the
player's folder with the id of one of the game's is skipped (to change the game's content, replace or
patch it, see below). The console's `mods` says where the folder is.

```json
{
    "id": "mymod",
    "name": "My mod",
    "version": "0.1.0",
    "api": 1,
    "dependencies": ["base"]
}
```

Mods load after their dependencies (`loadAfter` orders a mod after another one without needing it),
and in the order of their ids otherwise, whatever folder they are in.
A mod with a missing dependency, or in a dependency cycle, is skipped and reported.

**Turning a mod off**: `mods.json` in the save folder (next to the mods folder) lists the mods not to
load, `{ "off": ["wildfire"] }`; F5 reads it again. A mod that needs one turned off is skipped too. `api` is the
content API version the mod was written for. The API is unstable: a mismatch is only a warning.

### What a mod can hold

| File | Defines | ID |
|---|---|---|
| `blocks.json` | blocks: `color`, `solid`, `colorNoise`, `hardness`, `breakable`, `flammable`, `weakness`, `texture` (one name, or `{ "top", "side", "bottom" }`), `shape` (`cube`, `cross` for grass and flowers, `model` with a `model` for a small voxel model) | `mymod:<key>` |
| `statuses.json` | statuses: `duration`, `damagePerSecond`, `slow`, `maxStacks`, `atMax`, `stagger`, `heavy`, `imbue`, `damageTaken` (one with it takes this much more damage: -0.5 halves it) | `mymod:<key>` |
| `affixes.json` | what elites come with: `name`, `text`, `colour`, `weight`, `health`, `damage`, `speed`, `onHit` (a status its hits leave), `trail` (ground behind it), `regen`, `aura` (`status`, `radius`: on its allies), `blast` and `split` (as an enemy's) | `mymod:<key>` |
| `abilities/<name>.json` | an ability: terrain (`kind`: wall, pit, mine) or a weapon skill (attack); a run's technique also has `tags`, `levels` (a boost each: `[{ "damage": 0.15 }]`) and `branches` (`name`, `text`, `element`, `status`, `tags`, `boost`). `"slot"` (an ability key, 1-8: 4 is R, 8 is 4) puts it on that key and among a run's level ups by itself, no list to patch (`"levelUp": false` keeps it out of the level ups) | `mymod:<name>` |
| `elements.json` | the elements of spells: `name`, `status`, `mana` (fire, ice, volt) | `mymod:<key>` |
| `keywords.json` | the keywords of spells: forms (`shape`: bolt, zone, nova, beam) and modifiers | `mymod:<key>` |
| `spells/<name>.json` | a ready-made spell: `name`, `text`, `recipe` (`["fire", "bolt", "zone"]`), `levels` (what each level after the first adds: `[["enlarge"], ["extend"]]`), `branches` (`name`, `text`, `keywords` added to the recipe, `tags`, `boost`) | `mymod:<name>` |
| `upgrades.json` | the items of a run's shop: what a stat item adds (`maxHealth`, `damage`, `armor`, `crit`, `regen`, `pickupRange`, `dashCharges`, `horde`, `xpGain`, `shardChance`...; trade-offs: `healthScale`, `damageTaken`, any negative number, never scaled by a rarity; the moment's: `hurtDamage`, `airDamage`, `executeDamage`, `revengeDamage`; triggers: `thorns`, `killHeal`, `dodgeGround`, `proc` { `every`, `chains`, `damage`, `element`, `status` }, `landing` { `radius`, `damage` }) or a tag item does to everything of the player with its `tag` (`"boost": { "damage", "area", "cooldown", "cost", "duration", "speed", "count" }`), its `price`, how many one can have (`most`: none by default), the rarities it comes at (`tiers`: `["Rare"]`; none: any), its `look` | `mymod:<key>` |
| `run.json` | the arena run: waves, spawn pools and `spawnScale`, elites, bosses, XP, the start kit and spells, `levelSkills`, `levelRerolls`, `levelBanishes`, `branchLevel`, `mastery`, `presents`, `eliteAffixes` ([{ `from`, `count` }]), `intermission`, the shop (`priceScale`, `coopPrice`, `interest`, `rarities`), score, `damageFalloff`, `bossCoopHealth`, the `arenas` by wave and the duel's, the `difficulties` (each a `name`, `waves`, enemy `health`, `damage`, `healthGrowth`, `damageGrowth`, `spawn`, `eliteChance`; winning one opens the next) (the last mod with one wins) | |
| `arenas/<name>.json` | an arena for the run: `name`, `size`, `shapes`, `scatter` (decorations sown by chance: `[{ "block", "on", "chance" }]`), `playerSpawn`, `centre`, `spawnRadius` | `mymod:<name>` |
| `weapons/<name>.json` | a weapon and its moveset; in a run, its `levels` (a boost each: `[{ "damage": 0.2 }, { "attackSpeed": 0.1 }]`), `branches` (as a technique's) and `mana` (mana back per point of damage) | `mymod:<name>` |
| `enemies/<name>.json` | an enemy: stats, moveset, script, params; a boss's `phases` (`at`, `crystal`, `crystals`, `radius`, `stagger`); `anchored`: nothing pushes it (nor mends it); `blast` as it dies (`delay`, `radius`, `damage`, `poiseDamage`, `knockback`, `terrainDamage`, `element`, `status`, `everyone`: both sides); `split` as it dies in a run (`enemy`, `count`, `health`) | `mymod:<name>` |
| `models/<name>.vxl.json` | a voxel model (saved by the VoxelTest editor) | `mymod:<name>` |
| `textures/<name>.png` | a block texture (16 x 16 pixel art, any square size works); see-through pixels are not drawn | `mymod:<name>` |
| `scripts/<name>.pg` | an enemy brain | `mymod:<name>` |
| `tuning.json` | numbers merged over the tuning of earlier mods (`feel.gameSpeed`: the whole game's pace, 0.85 in the base mod) | |
| `arena.json` | the training arena (the last mod with one wins) | |
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
  `dies`: an enemy's attack that ends in its own death, and no reward for anyone (the sapper's fuse; its
  `blast` goes off as it dies).
- The moveset names `light`, `heavy`, `dodgeAttack`, `dashAttack`, `airLight` and `airHeavy`.

A weapon has a `type` (`dagger`, `sword`...). A weapon skill is an ability with `"kind": "attack"`,
an `attack` like a moveset's, and `weapons`, the types it works with. A slot of `player.abilities` can
be a list, `["base:shadow_dash", "base:whirlwind"]`: the first one the weapon in hand allows is there.

A spell keyword (`keywords.json`, the header comment explains every field) is a form, with its
numbers (`speed`, `radius`, `range`, `duration`, `damage`...), or a modifier, with the multipliers it
applies to its form (`count`, `durationScale`, `radiusScale`...) and the forms it can follow. Both
add `mana` and `cooldown`; a modifier's `manaScale` multiplies its form's cost. A bolt with `bounces`
skips off the floor that many times, keeping `bounciness` of its speed each time, and a wall, a ceiling
or an enemy stops it at once (`mods/wildfire` is a small example). A mod adds a keyword
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
| `elite <enemy> [affix ...]` | An elite in front of you with those affixes, or one picked as a run would (`elite brute blazing guardian`) |
| `clear`, `reset` | Remove the enemies; restart the arena |
| `weapon <id>`, `weapons` | Wield a weapon; list them |
| `view [first|third]` | Switch the camera |
| `reach [on\|off]` | Reach rings: how far each attack reaches, round whoever attacks |
| `god`, `ai`, `hitboxes` `[on\|off]`, `overlay` | Toggles (or set) |
| `item <id\|all> [rarity]`, `unitems` | Give yourself a shop item (rarity 0-3); take every item back |
| `grow <weapon\|technique> <level> [branch]` | Its levels and branch, as a run's level ups give them (branch 0 is the first, -1 none). They count in a run and in the lab |
| `stamina [on\|off]`, `mana [on\|off]` | Infinite stamina (and dash charges), infinite mana for the players (for testing moves and spells) |
| `pause`, `step [n]`, `slowmo <x>` | Freeze, advance n frames, change the speed (frame data checks) |
| `place x y z <block>`, `fill x0 y0 z0 x1 y1 z1 <block>` | Edit the arena (`air` digs) |
| `mods`, `errors`, `warnings` | What is loaded (and turned off), where the player's mods folder is; what went wrong |
| `players` | Who is in the game, and the round trip to the server |
| `spell <1-3> <recipe>` | Put a spell on a key: `spell 1 fire bolt multiply zone extend`, or a ready-made id: `spell 2 frost_nova`, or one grown: `spell 1 firebolt 5 0` (level 5, its first branch; not kept in the spellbook) |
| `spells`, `spellbook` | The spells on the keys, the ready-made ones and the keywords; open the spellbook |
| `arena`, `training`, `wave <n>` | Start the arena run, go back to training, jump to a wave of the run (and its arena) |
| `map [arena]` | Outside a run: the training ground becomes that arena (`map gorge`, `map training`); alone, the list |
| `timeup`, `kill`, `shards <n>` | End the wave now (a boss wave: its boss falls), kill every enemy (they drop their shards), get shards (and their XP) |
| `hurt <damage>` | Hit every enemy for that much, as a real hit lands (a boss stops at its phases, a shield takes nothing) |
| `pacer [n\|off [fps]]` | The engine's update pacing: n updates per drawn frame (2 by default), or off (as fast as it can, or capped at fps) |

Scripted input, for tests and for checking frame data: `mouse off` (ignore the real mouse and let go of the cursor), `click <x> <y>` (a click on the
UI, window pixels), `press <action>`, `hold <action> <frames>`,
`wait <frames>`, `sleep <seconds>`, `look <yaw> <pitch>`, `teleport x y z`, `where`, `echo <text>`.
For example:

```sh
./release/VoxelArena --exec "hitboxes; slowmo 0.25; teleport 37.5 4 24.4; wait 30; press light"
```

## The lab

A tool for making the game: it shows each item, weapon, technique, spell and enemy the mods load,
one at a time, on the training ground.

```sh
./release/VoxelArena --lab
./release/VoxelArena --lab --exec "lab show sword 5 0"   # straight to the sword at level 5, Flame blade
```

F3 (or the console's `lab`) shows and hides it in any game alone. The panel on the left has a tab for
each kind and a list of everything in it; picking one plays its scene:

- **Items**: the item on the player, at the rarity picked, the camera turning round it. The list
  says where each one is worn (`head`, `arm_l`...), `gem` for a tag item's gem, and `no look` for one
  that does not show yet.
- **Weapons**: in hand, at the level and branch picked, against three dummies: the light combo (a
  bow: a tap, then a full draw), the heavy, the dash attack, the air attacks, then each technique of
  the weapon.
- **Skills**: one technique at a level and branch (the weapon that uses it in hand), or a terrain
  ability (wall, pit, mine).
- **Spells**: a ready-made spell at a level and branch, cast twice at the dummies.
- **Enemies**: one coming for the player (who cannot be hurt), plain or as an elite with an affix.

Levels and branches count outside a run while the lab is open, so a branch's fire or a level's
bigger area shows as it would in a run. The player's own HUD is hidden; the enemies' bars, the damage
numbers and the attack warnings stay.

Keys: Up/Down pick, Left/Right the level (or the rarity), B the next branch, Page Up/Page Down the
tab, Enter plays it again, L loops it. A left drag over the arena turns the camera, the wheel zooms.
The buttons at the bottom: Replay, Loop, Slow (a quarter speed), Boxes (the hitboxes), Reach (each
attack's furthest reach, a ring on the ground: faint while it winds up, full while it can hit, in its
element's colour), Orbit, and the view (third or first person).

The camera stays yours: once you drag or zoom it, the next picks in that tab leave it where it is
(another tab places it again), and a drag turns Orbit off. Orbit, turned off or on, stays so from one
pick to the next; until you touch it, each scene decides (the items turn, the fights do not).

**The scenes are a pgscript**, `tools/lab/lab.pg`, read from the source tree: save it and the scene
plays again, no rebuild. The game calls `stage()` (the ground, the player at its mark), then one
function with what was picked as a table: `showItem(it)`, `showWeapon(w)`, `showTechnique(t)`,
`showSpell(s)` or `showEnemy(e)` (the top of the file lists their fields). A function answers with
console commands, played in order on the game's own steps:

```js
fun showSpell(s)
{
    spell(1, s.id, s.level, s.branch)   // "spell 1 base:firebolt 5 0"
    dummies("spell")
    cam(25, 16, 6.5)
    caption(s.name)
    press(s.action)                     // spell key 1
    wait(150)                           // 60 a second
}
```

`run(...)` takes any console command, its words joined; `wait`, `sleep`, `press`, `hold`, `caption`,
`cam`, `orbit`, `face`, `put`, `grow`, `spell`, `weapon`, `item` and a few more are there by name.
A script error shows in red in the panel until the file is fixed. The lab's own console commands:

| Command | What it does |
|---|---|
| `lab [on\|off]`, `lab show <id> [level] [branch]` | Show or hide the lab; open it on something (level 1: the first of its row, branch 0: the first) |
| `cam <yaw> <pitch> [distance]` | The third person camera, at once (yaw 0: from behind, 180: from the front) |
| `orbit [degrees a second]` | How fast a scene turns the camera round the player (0 stops); alone, the Orbit button: off or on for good |
| `face <yaw> [pitch]` | Where the player fights towards while the camera turns, and how far down its spells aim |
| `put <enemy> x y z [yaw] [affix ...]` | An enemy at a point; with affixes, an elite |
| `caption <text>` | A line over the arena: what the scene shows now |

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
