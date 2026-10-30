# Voxel Arena: UI spec (style B, "Edge Pips")

Companion to `UI_BRIEF.md`. Every number is in pixels at the **1280 × 720** base layout.
Rects are `x, y, w, h` from the top-left of the screen. "Right 24" means 24 px from the
right edge (`x = 1280 - 24 - w`). The mockups are on the design canvas; this file is
what to type into code.

---

## 1. Global rules

| Rule | Value |
|---|---|
| Scale | `s = min(winW / 1280, winH / 720)`. Multiply every number by `s`, then **round to whole pixels** so pips and borders stay crisp. 2560 × 1440 is exactly `s = 2`. |
| Letterbox | Centre the 1280 × 720 frame. Anchored elements (top-right, bottom-right…) stick to the real window edges, not the frame. |
| Corners | Square only. |
| Ink shadow | Every mark drawn over the world gets a hard copy in **Ink** offset **+2, +2** (text 46 px and up: +3, +3). Text = draw the string twice. No blur. |
| Pip / box border | A box with a "2 px ink border" = ink rect at full size, then the fill rect inset by 2 on every side. |
| Display font | **Jersey 15** (Google Fonts, OFL). Numbers, keys, titles, tags. |
| Text font | **Rubik** 500 / 600 / 700 (Google Fonts, OFL). Names, card text, labels. |
| Mono font | Keep the engine's monospace for the F1 overlay and the console only. |
| Key labels | Always data from the bindings (`[1]`, `[R]`, `ENTER`), never fixed text. |

## 2. Colours

| Token | Hex | Used for |
|---|---|---|
| Ink | `#0B0C10` | shadows, borders, empty-pip base |
| Empty pip | `#0B0C10` at 55 % | unfilled pips |
| Panel | `#101218` at 95 % | paused screens' main panel |
| Card | `#1C1F28` | cards, chips, rows, dark buttons |
| Card dim | `#15171E` | disabled / bought cards |
| Well | `#0F1116` | inset boxes (recipe, diagram, icon frames) |
| Line | `#3A3E4A` | dark-button border, dividers |
| Text | `#ECEEF2` | main text |
| Text dim | `#B9BECB` | secondary text |
| Muted | `#8E94A3` | labels, hints |
| Faint | `#6B7180` | key hints, locked |
| Gold | `#FFD678` | main buttons, keys, selection, level up |
| Gold ink | `#2A1E08` | text on gold |
| Health | `#D6403A` (number `#F2675F`) | |
| Stamina | `#56C46E` (exhausted `#C4A046`) | |
| Mana | `#548CEC` (no spell affordable `#3A5282`; cost text `#8DB4F5`) | |
| Poise | `#E8C454` | |
| Shards | `#BEA0FF` (price ink on gold `#6A4BC8`) | |
| Boss | `#E678FF` (enraged `#FF4FA0`, name `#FF8AC0`; tag ink `#2A0A30`) | |
| Warning | `#FF786E` | can't afford, spawn arrows, DOWN |
| Fire | `#FF6A3D` (text `#FF7A4D`) | |
| Ice | `#9EE8FF` | |
| Volt | `#FFE14A` | |
| Shop kinds | Stat `#96C8FF` · Weapon `#FFBE78` · Skill `#8CE6AA` · Element `#FF9696` · Spell form `#D2AAFF` · Modifier `#FF9EC8` | |

## 3. Icons

7 × 7 cell grid, one colour, drawn at 2, 3, 4 or 5 px per cell, plus an ink copy offset
+2, +2 when over the world. Each icon is a list of 7 strings (`#` = filled).
The set used in the mockups (copy the strings from the canvas files): wall, pit, mine,
whirlwind, leap, fire, ice, volt, lock, shard, skull, check, arrow left / right,
keen edge, beam, spear, chain, elite crown.

---

## 4. Combat HUD

### 4.1 Top-left: run (anchor top-left)

| Element | Rect / position | Style |
|---|---|---|
| `WAVE` label | x 24, baseline ≈ 46 | Rubik 700 12, tracking +2 |
| Wave number | x 70, y 14, h 41 | Jersey 46, Text; `/20` after it, Jersey 24, `#AEB4C2`, gap 4 |
| Timer | 14 px after `/20`, y 18 | Jersey 40, Text; **Gold from 5 s left**; boss wave: the word `BOSS` in Boss colour |
| Wave pips | x 24, y 62; 20 × (8 × 8), stride 11 | done Text, current Gold (Boss colour on a boss wave), rest ink 45 %; 2 px ink border |

### 4.2 Co-op team list (anchor top-left, only in co-op)

Starts x 24, y 92. One row per teammate, stride 18:
name Rubik 700 12 (w 58, Muted when down) · 10 pips 6 × 6 stride 7 (Health) ·
`DOWN · back next wave` Jersey 15 Warning.

### 4.3 Top-right: economy (anchor top-right, right 24)

| Element | Position | Style |
|---|---|---|
| Row 1 | top 14, right-aligned, items gap 18 | shard icon 7 × 4 px (28 × 28) + gap 7 + count Jersey 38 Shards · `LV n` Jersey 34 · gap 6 · `+N` badge Jersey 22 on Gold, padding 0 5, only when N > 0 |
| Row 2 | top ≈ 58, gap 10 | `SCORE n` Rubik 600 12 `#DDE0E8` · XP 12 pips 9 × 6 stride 11 (1 px ink border) · `35/60` Rubik 600 12 |

### 4.4 Boss bar (top centre, boss waves only)

| Element | Rect | Style |
|---|---|---|
| Name | centred on x 640, y 18, h 31 | Jersey 34, tracking +2, Text (enraged: `#FF8AC0`), shadow +3 |
| `ENRAGED` tag | 10 px right of the name | Jersey 18 on Boss, ink `#2A0A30`, padding 0 6 |
| Frame | 339, 53, 602, 18 | Ink |
| Segments | 20 × (28 × 14), first at 341, 55, stride 30 | full: Boss (enraged `#FF4FA0`); empty `#2A1630` |
| Half mark | 639, 49, 2, 26 | Text |
| Poise strip | 339, 75, 602, 3 | track ink 70 %, fill Poise |

### 4.5 Bottom-left: body (anchor bottom-left)

| Element | Rect | Style |
|---|---|---|
| Weapons row | x 24, y 600, h ≈ 18, gap 16 | Rubik 600 14; others Text dim; current Gold + 3 px Gold underline 1 px below; then `TAB` key: Jersey 15, 2 px Text border, padding 0 4 |
| Health number | 22, 628, 64, 50, right-aligned | Jersey 50, `#F2675F`, shadow +3. Value = current health |
| Health pips | 10 × (22 × 16), first at 96, 640, stride 25 | 1 pip = 10 % of max. Fill Health, empty ink 55 %, 2 px ink border |
| Stamina pips | 10 × (22 × 9), first at 96, 662, stride 25 | Stamina; **exhausted** = all filled pips `#C4A046` |
| Mana pips | 10 × (22 × 9), first at 96, 675, stride 25 | Mana; **no spell affordable** = `#3A5282` |
| Dash charges | 2–4 × (14 × 22), first at 352, 662, stride 17 | full Text; refilling: ink 55 % with `#9AA0AE` rising from the bottom, `h = 18 × progress` (1.2 s) |
| Under-bar line | x 96, y 690 | `EXHAUSTED` Jersey 17 `#E2C46E`, or player statuses (8 × 8 pips + name Rubik 700 11 in the element colour) |

### 4.6 Bottom-right: abilities (anchor bottom-right, right 24, right-aligned)

**Skills row**, top 630, items gap 14. Each item = keycap + gap 6 + name (+ gap 6 + seconds).

| Part | Size | Ready | On cooldown / charging |
|---|---|---|---|
| Keycap | 24 × 24, 2 px ink border, ink shadow +2 | fill Text, key Ink (Jersey 20) | ink base, Text rises from the bottom `h = 20 × progress`; key Text |
| Name | Rubik 600 14 | Text | `#9AA0AE` |
| Seconds | Jersey 20 | – | Text, one decimal (`3.2`); hold-to-charge shows `hold` |
| No stamina | | keycap border `#C4A046`, name `#9AA0AE`, cost `#E2C46E` | |

Terrain order: Q Wall · E Pit · G Mine, then the 2 weapon skills (R, 4).

**Spells row**, top 664, items gap 16.

| Part | Size | Ready | Cooldown | Not enough mana | Locked |
|---|---|---|---|---|---|
| Keycap | 28 × 28, 2 px border | element colour full, key Ink (Jersey 22) | element colour rises `h = 24 × progress`, key Text | fill `#3A5282`, border Warning | ink 50 %, border Faint, key Muted |
| Name | Rubik 700 15 | element text colour | element colour dimmed (`#C9876F` for fire) | element colour | Muted |
| Right | Jersey 20 | mana cost `#8DB4F5` | seconds Text | cost Warning | `locked` Muted |

### 4.7 Crosshair (centre 640, 360)

Four corner brackets, each a 6 × 2 and a 2 × 6 rect, Text with a 1 px ink outline:
top-left at (630, 350), top-right at (644, 350) / (648, 350), bottom-left at (630, 368) / (630, 364),
bottom-right at (644, 368) / (648, 364). Centre dot 2 × 2 at (639, 359).

### 4.8 Enemy bars (world-anchored, up to 32)

Anchor: centred over the enemy's head, bottom 6 px above it.

| Part | Rect (relative to bar's top-left) | Style |
|---|---|---|
| Frame | 0, 0, 66, 8 | Ink; **elite** Gold; **boss (small)** Boss |
| Cubes | 8 × (7 × 6), first at 1, 1, stride 8 | Health; empty ink 65 % (boss: Boss colour) |
| Poise | 0, 9, 66, 2 | track ink 70 %, fill Poise, width = 66 × poise |
| `STAGGERED` | centred, y −20 | Jersey 16 Poise, tracking +1, while poise is broken |
| Statuses | first at 70, 0; 8 × 8 stride 10, 1 px ink outline | one square per stack: burn Fire, chill Ice (1–4), frozen = white / Ice / white, shock Volt |

### 4.9 Off-screen markers (up to 10)

Arrow icon 7 × 7 at 5 px (35 × 35) + ink copy +2, +2. 12 px in from the screen edge,
y clamped to 80 … 560. Spawn = Warning. Boss = Boss, with the name (Jersey 18) and
distance (Rubik 700 11) beside it, on the screen side.

### 4.10 Moments (timings to implement; drawn only where noted)

| Moment | What happens |
|---|---|
| Hit taken | Each lost health pip detaches: drops 20 px over 0.4 s, alpha 0.6 → 0. Red wash: full screen `#D6403A`, alpha 0.35 × hit-flash setting, fades out in 0.3 s. |
| Low health (≤ 30 %) | 8 px Health frame on all four screen edges (drawn), pulsing alpha 1 ↔ 0.5 at 2 Hz. |
| Elite arrives | Its spawn arrow is Gold instead of Warning; gold frame on its bar from spawn. |
| Boss enraged (< 50 %) | Bar and name switch to enraged colours, `ENRAGED` tag, 6 px Boss lines on the top and bottom edges (drawn). |
| Level up | The `+N` badge pops (scale 1.4 → 1 over 0.2 s); `LEVEL UP` Jersey 56 Gold, centred at y 200, 1 s. |
| Spell off cooldown | Keycap flashes Text for 2 frames, then settles to the element colour. |
| Last seconds of a wave | Timer Gold from 5 s. `3`, `2`, `1` Jersey 96 Gold, centred at y 190 (above the crosshair), each 1 s. |
| Centre messages | Wave name or boss name: Jersey 56, centred at y 200, 1.5 s. `Paused`: Jersey 56 at y 300 over ink 55 %. |

---

## 5. Paused screens: shared parts

| Part | Spec |
|---|---|
| Backdrop | full screen ink at 55–60 % over the live world |
| Panel | Panel colour, 2 px ink border, ink shadow +4, +4 |
| Card | Card colour, 2 px ink border, ink shadow +3, +3; kind = 8 × 8 square + label Rubik 700 11 tracking +1.5 in the kind colour |
| Gold button (main) | Gold fill, Gold-ink text, 2 px ink border, ink shadow +3 |
| Dark button | Card fill, Text, 2 px Line border |
| Disabled / bought | Card dim, Muted text, dashed or no shadow |
| Can't afford | button dark, price Warning |
| Selected / hover / kept | 2 px Gold border |
| Section label | Rubik 700 11, tracking +1.5, Muted or Text dim |

## 6. Shop

Panel **40, 28, 1200, 664** (padding 22 top/bottom, 24 sides → content 66 … 1214 × 52 … 668).

| Element | Rect | Notes |
|---|---|---|
| Title | 66, 52, –, 42 | `WAVE 9 OF 20 SURVIVED` Jersey 40, tracking +1 |
| Boss tag | 14 px after title, h 24 | bg `#2E1636`, skull 7 × 2 px + `next: a boss` Rubik 700 12 Boss |
| Header chips | right-aligned to 1214, h 42, gap 14 | Card fill: shards (icon 3 px + Jersey 30) · `Lv n` Jersey 28 + 12 XP pips 6 × 6 + `28/88 to Lv 7` Rubik 600 10 · `SCORE` label + Jersey 28 |
| Level-up strip | 66, 108, 872, 64 | bg `#2A2414`, 2 px Gold border; `LEVEL UP! +2` Jersey 28 Gold + `A stat item, free (2 to pick)`; 3 picks share the rest, h 46, bg `#3A321C`, gap 10; key badge 22 × 22 Gold |
| Offer cards | y 186, h 416; w 209; x 66 / 287 / 508 / 729 | padding 14, gap 10: kind row · icon frame 52 × 52 (Well, 2 px kind-colour border, icon 4 px) · name Jersey 30 · text Rubik 13 / 1.4 · Buy 40 h · Lock 30 h |
| Buy button | card bottom − 14 − 30 − 10 − 40 | Gold: key cap 20 × 20 (ink, Gold key) + `Buy` Rubik 700 14 + price Jersey 22 right. Bought: `#16261C` fill, `#8AE59E` check + `BOUGHT` |
| Lock button | card bottom − 14 − 30 | Card dim + lock 2 px + `Lock` + `⇧n`. Kept: Gold fill, `Kept for next shop`, card border Gold |
| Build summary | 954, 108, 260, 494 | Card. Rows h 22: label Rubik 13, value Jersey 20 right (`12% / 70` with `/ 70` Faint). Divider, then `OWNED` tags h 20 (Well, 6 × 6 kind square) |
| Reroll | 66, 621, ≈ 250, 42 | dark button: `R` cap + `Reroll` + price Jersey 22 Shards + `next 18` |
| Ready | 954, 616, 260, 52 | Gold, `READY` Jersey 30 tracking +2 + `ENTER` cap; co-op waiting: `WAITING…` + `1 / 3`, `#3A3E4A` fill |
| Keys | 1–4 buy · 5–7 level-up picks · Shift + n lock · R reroll · Enter ready | |

## 7. Spellbook

Panel **64, 32, 1152, 656** (padding 20 / 24 → content 90 … 1190 × 54 … 666).

| Element | Rect | Notes |
|---|---|---|
| Header | 90, 54, 1100, 36 | `Spellbook` Jersey 40 · `PAUSED` tag · hint Rubik 12 Muted · `[B] Close` dark button h 30 right |
| Key tabs | y 104, h 40; 3 × 361, gap 8 | key cap 24 × 24 + spell name (element colour) + right meta. Editing: Gold border, `EDITING` Gold |
| Recipe well | 90, 158, 784, 112 | Well. Blocks in a row, gap 6, `>` Jersey 28 Faint between forms. Element 96 × 60 filled element colour. Form 96 × 60 Card, 5 px Text top band. Modifier 72 × 48 attached to its form, `#2A2340`, 4 px Spell-form band. Next slot 72 × 48 dashed `#4A4F5C` |
| Readout | 890, 158, 300, 112 | Card. `MANA` / `COOLDOWN` / `DAMAGE` labels + Jersey 36 values; last change `+8` `#8AE59E`; one plain sentence Rubik 13 |
| Parts | 90, 284, 784, 180 | 3 rows (element / forms / modifiers) at y 284, 342, 400. Label column 86 w. Tiles 98 × 48, gap 6, 4 px band in the group colour, key cap 16 × 16 top-right, name Jersey 22, note Rubik 600 10. In use: Gold-ish border in element colour. Not allowed: alpha 0.45 + reason. Not bought: Warning `not bought`. Hover: Gold border + what it would do |
| Diagram | 890, 284, 300, 180 | Well, dot grid 2 × 2 every 24 px `#22252D`. You 16 × 16 Text. Trails of squares 4 → 8 px in the element colour. Zones = element colour at 28 % + 2 px border. Labels Jersey 16 |
| Actions | 90, 478, 1100, 32 | `[Backspace] Remove last` · `[Del] Clear` · `NAME` + input 200 w · `[Enter] Save to book` (Gold) · status right Rubik 600 12 `#8AE59E` |
| Book | label at y 524; grid 90, 544, 1100, 94 | 5 columns × 216 w, rows 44 h, gap 6. Card with 4 px left band in the element colour; name Rubik 700 13; recipe Rubik 500 10 Muted |
| Part keys (default) | elements Z X C · forms A S D F · modifiers Q W E R T | rebindable |

## 8. Title screen

No panel. Live arena behind.

| Element | Rect | Notes |
|---|---|---|
| Left band | 0, 0, 620, 720 | ink 82 %, then two 8 px steps: 620–628 ink 60 %, 628–636 ink 35 % (hard-edged fade) |
| Logo blocks | first at 72, 72; 8 × (18 × 18), stride 23 | Health, Stamina, Mana, Gold, Shards, Fire, Ice, Volt; 2 px ink border |
| `VOXEL` / `ARENA` | x 72, y ≈ 100 / ≈ 190 | Jersey 112, line 0.8, tracking +2; ARENA in Gold; shadow +5 |
| The Arena | 72, 318, 472, 78 | Gold button: `ENTER` cap (ink, Gold) + `The Arena` Jersey 34 + one line Rubik 500 13 `#5A4718` |
| Other items | 72, 404 / 460 / 516, 472, 48 | dark button: key cap 26 × 26 Gold + name Jersey 28 + hint right Rubik 12 Muted |
| Waiting for host | replaces The Arena, same rect | Card, 2 px dashed `#4A4F5C`; 3 loading squares 10 × 10 + `Waiting for the host…` Jersey 30 + `3 of 8 players in · only the host can start` |
| Best score | x 72, y 610 | `BEST SCORE` label · Jersey 40 · detail Rubik 12 Muted |
| Hint | x 72, y 668 | Rubik 11 Faint |

## 9. End of run

Backdrop: ink 50 % (Victory) or `#280A0A` at 60 % (Defeated).
Panel **320, 150, 640, 420**, top border 6 px in the accent (Victory Gold, Defeated `#F2675F`);
content 354 … 926 × 184 … 540.

| Element | Rect | Notes |
|---|---|---|
| Title | 354, 184 | `VICTORY` / `DEFEATED` Jersey 84, accent, shadow +4 |
| Subtitle | below, gap 6 | Rubik 600 16 `#DDE0E8` |
| Wave pips | y ≈ 294; 20 × (26 × 10), stride 29 | reached Text, death wave Health, rest ink 60 % |
| Score box | 354, ≈ 322, 572, ≈ 95 | Well. `SCORE` + Jersey 56; `BEST!` Gold tag or `(best 7 073)` Muted; right: KILLS / LEVEL / TIME, 86 w each, Jersey 34 |
| Play again | 354, 492, –, 48 | Gold, `ENTER` cap + Jersey 28 |
| Training ground | next to it, h 48 | dark, `T` cap; solo only (hide in co-op) |

## 10. Options

Panel **320, 44, 640, 632** (padding 22 / 26 → content 348 … 932 × 68 … 652).

| Element | Rect | Notes |
|---|---|---|
| Title | 348, 68 | `Options` Jersey 44 + `saved as you change it` Rubik 12 `#8AE59E` |
| Rows | 348, 122 + 52 × i, 584, 46 (9 rows) | label Rubik 600 15 (w 200). Focused row: bg `#252833` + 4 px Gold left bar |
| Number rows | | 10 pips 12 × 14 stride 14 · value Jersey 24 right-aligned in 76 · `−` `+` buttons 32 × 32 dark, right |
| Two-way rows | | segmented, h 32, padding 0 14, Jersey 20: selected Text fill / Ink text, other Card / Muted |
| Back | 348, 610, –, 42 | Gold, `ESC` cap + `Back` Jersey 24; hint Rubik 11 Faint |

Rows in order: Mouse sensitivity · Invert mouse Y · Field of view · Volume · Camera shake ·
Hit flash · Slash trails · View (First person / Third person) · Debug overlay (F1).

## 11. Developer layers (unchanged look, kept functional)

F1 overlay: engine monospace 12 Text on ink 70 %, top-left 8, 8. Console: 0, 490, 1280, 230,
ink 92 %, 2 px Gold top line, lines monospace 13, input line at the bottom with a `>` in Gold.
Content-error banner: 0, 0, 1280, 28, Health fill, Rubik 700 13 Text, sits above everything.
