# Voxel Arena: UI design brief

## The game in one paragraph

Voxel Arena is a fast action game you play in first person (third person is an option), built out of
blocks. A run is **20 timed waves** in a 40 x 40 m pit with four pillars, a platform and a boulder
hanging from a rope. Enemies come in from the edges for the whole wave: slimes first, then skulkers,
archers and brutes, then gold-glowing **elites**. A boss arrives on wave 10 (the Brute Chieftain) and
on wave 20 (the Stone Warden). Enemies drop **shards**, which count as both money and XP. Between
waves there's a **shop**, and each level up gives a free stat item. The player fights with three
weapons (dagger, sword, spear) that have combos and skills, and with **spells they put together
themselves** (an element, then keywords: `Fire > Bolt > Zone`). Movement goes a long way: rolls with
invulnerability, dash charges, double jumps, wall jumps, air dashes. The arena can be broken: you can
raise walls, dig pits, mine blocks, burn ropes and drop the boulder. Co-op for 2 to 8 players.

It's played with keyboard and mouse at a fast pace, so the in-combat HUD has to be readable at a
glance, and the shop and spellbook can be denser because the game waits there (the shop is between
waves, and the spellbook pauses solo play).

## Look and tone

- The world is chunky voxels in flat colours, with soft corner darkening. There are no textures.
- The current UI is dark translucent panels (`#0C0E16` at about 65-90% opacity), off-white text
  (`#ECEEF2`), and gold (`#FFD678`) to highlight keys and choices. It works but it's plain. I'd like
  a stronger identity that suits the blocky world (square corners, pixel or blocky shapes, chunky
  pips rather than smooth gradients), and it has to stay clean, not cluttered.
- Colours already in use, which can be changed:
  - Health `#D6403A`, stamina `#56C46E` (amber `#C4A046` when exhausted), mana `#548CEC` (dimmed
    `#3A5282` when no spell is affordable), poise `#E8C454`
  - Shards `#BEA0FF`, boss `#E678FF`, warnings/unaffordable `#FF786E`
  - Shop card kinds: Stat `#96C8FF`, Weapon `#FFBE78`, Skill `#8CE6AA`, Element `#FF9696`,
    Spell keyword `#D2AAFF`
- Spell elements need their own colours that are easy to tell apart: **Fire** (burn), **Ice**
  (chill, then frozen at 4 stacks), **Volt** (shock).

## Direction and references

- **One idea for the whole UI: it's made of the same voxels as the world.** Panels are carved blocks
  with a one-block bevel. Bars are rows of cubes that pop off as you take damage (for example, one
  cube per 10 health). Shards are little crystal voxels, and dash charges are solid blocks. The
  spellbook is a rune tablet: each keyword is a block you slot into a row, and the chain diagram is
  those blocks lining up. Every choice should look like it was built from the arena.
- **Lean into the engine's limits.** Square corners only. Borders and bevels are built from offset
  rectangles, with hard block shadows and flat fills. Use stepped, pixel-like shapes rather than
  curves.
- **Take the palette from the world.** The arena's stone greys, earth browns and plank tans make the
  frames. The stat colours (health, stamina, mana, shards) are the only saturated colours, so they
  stand out. Each element gets one strong colour: fire orange-red, ice pale cyan, volt electric
  yellow.
- **The type has character.** A chunky pixel or blocky display font for titles, numbers and key
  labels, and a plain readable one for card text. Any TTF font works. Suggest a pair.
- **References, one thing from each:**
  - Risk of Rain 2: how readable the HUD is in a busy fight, with everything at the edges.
  - Hades: the boss bar and the boss's name.
  - Brotato: a dense shop of cards and a stat summary that you can still read quickly.
  - Noita: the feeling of building a spell out of parts.
  - Minecraft Dungeons: blocky, chunky UI that is still clean.
  - Take what's described, not the look of any one of them.
- **Avoid:**
  - glassmorphism, blur and frosted panels;
  - neon cyan sci-fi;
  - rounded cards with soft drop shadows;
  - gradient buttons;
  - progress bars that look like a web app;
  - generic sword-and-shield or gear icons;
  - anything that reads as a dashboard or a mobile game store.
- **Design the moments, not just still screens:**
  - low health,
  - a hit taken (the cubes falling off the bar),
  - an elite arriving,
  - the boss getting angry under half health,
  - a level up,
  - a spell going off cooldown,
  - the last 3 seconds of a wave.
- **Work over the screenshots of the real game** that come with this brief, at their real size. A
  panel that looks good on black can disappear over a stone floor full of slimes.

## Screenshots of the game as it is (1280 x 720, in `ui_brief_shots/`)

| File | What it shows |
|---|---|
| `01-title.png` | The title screen over the arena (the training ground), with the first-person sword |
| `02-wave-fight.png` | Wave 8: a brute winding up its slam close by (the marker over its head), archers and a slime further off, enemy bars |
| `03-boss-wave.png` | Wave 10: the Brute Chieftain's bar at the top, `BOSS` in place of the timer |
| `04-third-person.png` | Third person in a busy wave: the player's back, lots of enemy bars at once |
| `05-shop-levelup.png` | The shop with 4 offers, 6 level ups to pick, the build summary, Reroll and Ready |
| `06-spellbook.png` | The spellbook: the 3 keys, the recipe being built, element, forms, modifiers, ready-made spells |
| `07-options.png` | The options screen |
| `08-victory.png` | The end of a run (Victory) |
| `09-training.png` | The training ground: a brute winding up (red glow, marker, the zone on the ground), slimes, archers, a dummy |
| `10-you-fell.png` | Dead in the training ground: the empty health bar, `You fell` |

They show the current UI, so everything in them can be redesigned. Use them for the colours, light
and clutter of the world the UI sits on.

## How to go about it

1. Start with **three very different directions for the combat HUD alone**, each over the same
   gameplay screenshot.
2. Once one is picked, lock its style (palette, type, panel, bar and button) as a small style sheet.
3. Design the rest of the screens in that style: the spellbook, then the shop, then title, end of
   run and options.
4. For each final screen, give **pixel positions and sizes at 1280 x 720 and hex colours**. The HUD
   is placed by hand in code.

## Technical constraints (please design within these)

- Base layout is **1280 x 720**, and the whole UI scales with the window. Designs should hold up from
  1280 x 720 to 2560 x 1440.
- The engine draws the UI with **solid-colour rectangles and TTF text**: two fonts, a proportional
  one and a monospace one, and colour can change in the middle of a line. Rectangles, bars, pips,
  borders made of rectangles and blocky icons are cheap to build. Image sprites, rounded corners,
  blur and gradients would be new engine work, so please use them only where they really pay off.
- Everything is clickable with the mouse **and** has a keyboard shortcut, and the shortcut should be
  shown on the control (for example `[1]`, `[R]`, `[Enter]`). Key bindings can be changed, so the
  label is data, not fixed text.
- First person is the default view, so the screen centre (the crosshair) and the area around it have
  to stay clear.

## Screens to design

### 1. Title screen
Shows the game name, then **Enter**: The Arena (20 timed waves, a shop between them, bosses midway
and at the end), **T**: Training ground, **B**: Spellbook, **O**: Options, and the **best score**.
The arena is rendered live behind it. In multiplayer only the host can start, so clients need a
"waiting for the host" state.

### 2. Combat HUD (the most important one)
Everything that's on screen during a wave:
- **Health, stamina, mana bars** (bottom left today). Stamina turns amber when *exhausted*. Mana
  dims when no equipped spell is affordable.
- **Dash charges**: 2 pips by default, up to 4 with upgrades. The one refilling is shown partly
  filled (one comes back every 1.2 s).
- **Weapons owned**, with the current one highlighted, and the *next weapon* key (Tab).
- **Ability bar**: the 3 spell keys (spell name in its element's colour, plus ready / on cooldown /
  not enough mana / `(locked)` until its parts are bought), the 2 weapon skills on R and 4
  (stamina and cooldown, with a hold-to-charge one: shadow dash), and the terrain abilities
  (Q raise wall, E dig pit, G mine).
- **Crosshair**: abilities and spells aim with it. The block it's on gets an outline in the world
  when an ability can reach it.
- **Top line**: `Wave 7/20`, the wave timer (which turns gold in the last 5 s, and is replaced by
  `BOSS` on a boss wave), shards, `Lv 4` with XP progress (`35/60`) and `+N` unspent level ups,
  and the score.
- **Boss bar**: the boss's name and a wide health bar at the top. The Stone Warden gets angry under
  half health, which could be shown.
- **Enemy bars**: a small health bar over each enemy with a thin **poise** strip under it, and a
  *staggered* state when poise breaks. Elites are gold. Up to 32 can be on screen.
- **Off-screen spawn arrows**: a spawn point glows red just before an enemy comes in. If it's out of
  view, an arrow at the screen edge points to it (a trail of shrinking squares today). Up to 10.
- **Hit feedback**: a red wash over the screen when you're hit, fading in about 0.3 s, with
  adjustable strength.
- **Centre messages**: the wave's name as it starts (or the boss's name), a 3-2-1 countdown at the
  end of a wave, `Paused`, and `You fell: back up next wave` (co-op).
- Possibly new: status icons on the player and on enemies (burn, chill stacks 1-4, frozen, shock),
  and a co-op teammate list (names, health, down/alive).

### 3. Shop (between waves, one per player)
- Header: `Wave 7 of 20 survived` (+ `next: a boss` when it is), shards, level and XP to the next,
  and the score.
- **4 offer cards**. Each has a *kind* (Stat / Weapon / Skill / Element / Spell form / Modifier,
  colour-coded), a name, a short line of text, `(have N)` for stacked stats, a **Buy** button with
  the key and price (red when unaffordable, `Bought` once bought) and a **Lock** toggle (keeps that
  offer for the next shop). Prices go up each wave.
- **Level up!** row: when there are unspent level ups, a choice of free stat items
  (`A stat item, free (2 to pick)`).
- **Build summary**: Health, Damage +%, Armor % (70% at most), Crit %, Speed +%, Regen /s, Dashes,
  Pickup range, and the list of everything owned.
- **Reroll** button `[R]`, which costs more each time, and **Ready** `[Enter]`. In co-op that
  becomes `Waiting for 2...` until everyone is ready.
- The 14 stat items as examples: Tough (+25 max health), Keen edge (+15% damage), Nimble (+1 dash
  charge), Focus (spells cost 15% less), Wellspring (+40% mana regen), Second wind (+35% stamina
  regen), Swift (+8% move speed), Quick hands (20% faster recharge), Leech (heal 2 on hit), Potent
  (statuses 1 stack deeper), Plated (6% less damage), Precise (+6% crit), Magnet (+1.5 m pickup),
  Mending (0.6 health/s).

### 4. Spellbook (B)
This is where the player builds spells. It's the most unusual screen and the one to get right.
- Pick the **key** being edited (1, 2, 3).
- Build a recipe in order: **one element** (Fire, Ice, Volt), then **keywords**:
  - *Forms*, the shapes a spell takes: **Bolt** (a shot), **Zone** (a lingering area on the ground),
    **Nova** (a burst all around), **Beam** (an instant strike along the aim). The first form leaves
    from you, and each later form comes out of the one before it (where the bolt lands, where the
    beam strikes...).
  - *Modifiers*, which change the form just before them: **Multiply** (x2), **Extend** (longer
    lasting / longer reach), **Enlarge** (bigger), **Chain** (jumps to 2 more enemies, bolts and
    beams only), **Empower** (harder hitting).
- What can't come next is greyed out. In a run, anything not bought yet is greyed out too.
- While building, the **mana cost**, the **cooldown** and a plain-language description of what the
  spell does update live. A diagram of the chain (bolt → zone, x2) would help a lot.
- Remove the last word, clear, **name and save** to the book, and a list of **ready-made and saved
  spells** to load onto the key: Firebolt (fire bolt zone), Fireball (fire bolt nova), Twin embers
  (fire bolt multiply zone extend), Ice shards (ice bolt multiply multiply), Frost nova (ice nova
  enlarge), Glacier (ice zone enlarge extend), Chain lightning (volt beam chain), Thunderclap (volt
  beam nova).
- A status line for the last thing that happened ("saved").

### 5. Options (O)
Each row has -/+ or a toggle: mouse sensitivity, invert Y, field of view, volume, camera shake,
hit flash strength, slash trails on/off, first/third person, debug overlay. There's a **Back**
button.

### 6. End of run: Victory / Defeated
`Victory`: all 20 waves cleared and the Stone Warden is down. Or `Defeated at wave 12 of 20`. Then
the score (with `best!` or `(best N)`), kills, level, and time, and **Enter** to play again or **T**
for the training ground (solo only).

### 7. Training ground
The same combat HUD with no run line, no timer and no shop. Dummies and enemies come back after
they die. Nice to have: a small panel with the sandbox toggles (infinite stamina, infinite mana) and
a damage readout.

### 8. Multiplayer states
`Connecting...`, `Joining...`, `Refused: <reason>` (for example, different content), and
`Connection lost`. Also *fallen, back up next wave*, waiting for others to be ready in the shop, and
player names over teammates.

### 9. Developer layers (low priority, keep them functional)
These are a monospace debug overlay (F1), a drop-down console (`` ` ``) that takes the bottom 230 px
and shows text lines plus an input line, and a red banner at the top for content errors.

## What I'd like back

1. A visual direction: palette, type, how panels, bars and buttons look, and an icon style that
   suits a voxel game.
2. Mockups at 1280 x 720 of the combat HUD (a normal wave, then a boss wave with lots of enemies),
   the shop (with a level up pending), the spellbook (in the middle of building a spell), the title
   screen and the end-of-run screen.
3. The states listed above: on cooldown, unaffordable, locked, bought, exhausted, mana low,
   staggered, elite.
