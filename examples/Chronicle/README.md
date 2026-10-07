# Chronicle

The Chronicle UI kit, built on ColumbaEngine. Everything here lives in the
`chronicle` namespace; engine types are used as `pg::…`.

## Running

Run from the repo root (assets are read relative to the working directory):

```
./release/Chronicle                           # the Life scene, from save/chronicle/life.sz (the mockup's life when there is none)
./release/Chronicle --no-save                 # the mockup's life, never written
./release/Chronicle --fresh                   # a new life at 7
./release/Chronicle --save save/other.sz      # another save file
./release/Chronicle --month-ms 1000           # a month every second instead of every 2
./release/Chronicle --dev TypeSpecimen --theme candle
```

With no `--dev` the game opens on the **Life scene** (`--dev LifeScene` names it too).
`--dev <Scene>` picks a dev gallery instead; an unknown name exits with code 2 before a
window opens. On the Life scene, confirming an activity starts the months (one every 2 s,
`--month-ms`), and they stop when its term ends, waiting for the next choice. The running
row's rule fills through each month in the month's own time. `SPACE` pauses or resumes work
under way (a paused month keeps what it ran), and the head of *At work now* says which:
`RUNNING`, `PAUSED · SPACE` in red, or `IDLE` between two works. At nothing no key runs the
months: *Pass a month*, the button standing in *At work now* in the running row's place, passes
one. `M` passes one month at any time (a dev key), `T` switches the theme, `R` reduced motion, `S` saves, `N` starts
a new life. The life is also saved on its own, with no line in the log: after every month, every choice, every thing done
at once and every new life (never with `--no-save`); `S` writes it and says so. A life whose Vitality reaches 0 is lost: its **ending** comes up over the page, a leaf
saying who he was, how it ended and what the chronicle keeps of him, and nothing passes until
*Begin a new life* (or `N`). The next one begins at 7 with 12 rations, the
months stopped and a line in its log saying how the last one ended. His prime ends at 30: every
activity of it closes, only an old man's work and the market are left, and from there the most
his Vitality can be falls by one every two months, until that life ends too. Vitality stands under the
clock's age at the top of the right column, his other parts in a panel under the log. A month
that takes from it turns it red for a moment, and the first such month stops the running
months, with a line in the log naming the part and the key that goes on. What was taken mends, one every two months, up to the most it can be: the hatch on its
line and the `-> 12` beside its figure.

Tests:

```
cmake --build . --target test_chronicle
ctest -R "theme" --output-on-failure
```

## Web build

Chronicle also builds with Emscripten, as the `Chronicle` target of a web build tree
(`em/` here; any directory configured with `emcmake cmake -DCMAKE_BUILD_TYPE=Release ..`):

```bash
cd em
make Chronicle
emrun Chronicle.html
```

To publish, `make ChronicleWeb` builds the game and packs `chronicle-web.zip` (and the same
files in `chronicle-web/`, to try with `emrun chronicle-web/index.html`): the page made from
`web/index.html.in` with `Chronicle.js`, `Chronicle.wasm` and `Chronicle.data`. The page's canvas fills the frame
and the game is resized to it; it keeps black behind the canvas, since a light ground shows
through every blended pixel and fades the text. The page asks for the three files under a stamp made from them
(`web/packpage.cmake`, `Chronicle.js?v=...`): a host may let browsers keep the `.js` for hours and
not the `.wasm`, and a new one would then meet the other of the build before and fail to link.
`Chronicle.html` is Emscripten's test page and
is not published. On itch.io the project has to have *SharedArrayBuffer support* ticked in its
frame options.

- **The pack** (`Chronicle.data`) holds what the game reads at run time, taken from the
  source tree under the same paths: the shaders, the boot scripts, the two font families,
  the icon sets, `res/chronicle/` (tokens, manifest, pages) and `rules/`. The list is
  `CHRONICLE_WEB_FILES` in `CMakeLists.txt`; a file read from a new place has to be added
  there. Nothing comes from the `res/` and `shader/` copied into the build directory.
- **Rules** are packed as `.pg` only and compile in the browser, never from a `.pgc`.
- **Relinking**: the pack is made at link time, and editing a packed file relinks the target.
- **Threads**: the build uses pthreads, so the page must be cross-origin isolated
  (`Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`).
  `emrun` sends both; another host has to be set up to. A page framed by a site that is not
  isolated itself (galaxy.click) can never be: that is what the build without threads is for.
- **Without threads**: a second build tree configured with `-DWEB_THREADS=OFF` builds the
  engine with `PG_NO_THREADS`, and the same `make ChronicleWeb` packs a game that starts on any
  page, framed or not, with no header to send:

  ```bash
  emcmake cmake -S . -B em-nothreads -DCMAKE_BUILD_TYPE=Release -DWEB_THREADS=OFF
  cd em-nothreads
  make ChronicleWeb
  ```

  There the systems run in the frame callback (`src/Engine/ECS/serialtaskflow.h` stands in for
  taskflow, a few passes a frame), the window is made without the init thread, and the save
  folder is kept in IndexedDB instead of the origin private file system, so a save does not
  pass from one build to the other. The game is the same.
- **Save**: `save/chronicle/life.sz` is kept in the browser's origin private file system
  (in IndexedDB without threads), per site and per browser. The launch flags of the native build have no equivalent yet.
  With no save a web player begins a new life at 7 (`LaunchOptions::freshWithoutSave`), not
  the mockup's life the native build opens on. The life is saved on its own as it goes, so a
  closed tab loses a month at most.

## Analytics

`Core/analytics.h`. From a browser the game tells the analytics proxy (the Cloudflare worker
that holds the database, the one GameDevJs2026 uses, `Analytics::ProxyUrl`) how long it is
played and where the life stood when the player stopped. A native build sends nothing, and
neither does a page served from `localhost` or `127.0.0.1`: it writes in the console what it
would have sent.

One row an event, in the `analytics_events` table of `analytics-server/schema.sql`, the events
of this game told apart by their `chronicle.` prefix:

| event | when | what the row says |
|---|---|---|
| `chronicle.session_start` | the first pass | the time already played in this browser |
| `chronicle.session_end` | every time the page is hidden or closed | the session's time, the total, the digest |
| `chronicle.life_end` | a life is lost | the same, at his death |

- **The session's id** is random and new for every page load: it ties the rows of one session
  and says nothing of the player. The last `session_end` of an id is where he stopped.
- **The time** is the time the page was in view, counted by the browser's clock between the
  moments it is shown and hidden. `total_play_time_ms` adds the sessions before, kept with the
  systems' save.
- **The digest** (`LifeSave::digest`, in `save_snapshot`) is one line of JSON:
  `{"age":12.50,"world":66,"aim":"warrior","running":"yard","monthsIn":2,"terms":14,"deeds":2,"log":31}`.
  The scene gives it after every month, choice, thing done at once and new life.

```sql
-- How long a session lasts, and how old he was when they stopped
SELECT session_id, MAX(session_duration_ms) / 60000.0 AS minutes,
       (ARRAY_AGG(save_snapshot ORDER BY timestamp_ms DESC))[1] AS stopped_at
FROM analytics_events WHERE event_type = 'chronicle.session_end'
GROUP BY session_id ORDER BY MIN(timestamp_ms) DESC;
```

## Layout

- `Core/` — data with no rendering: `textmetrics` (ascender and baseline helpers
  over the theme's text styles), `motion`. The tokens, text styles, font files
  and elements live in `res/chronicle/tokens.json`, loaded by the engine's
  `ThemeSystem` (`src/Engine/UI/themesystem.h`): a part is painted by attaching a
  `ThemeComponent` with an element key (`panel.ground`, `label.body.ink-muted`,
  `button.seal.ground.hover`), and `theme->setTheme("candle")` repaints everything.
- `Scenes/` — `LifeScene` (the game's screen, `lifescene.h`, with its save in
  `lifesave.h`) and the dev galleries. `TypeSpecimen` shows every style on vellum.
- `UI/` — components (added from phase 1.2 onward).

## Rules

- **`chronicle` namespace.** Everything under `examples/Chronicle/` is in
  `chronicle`; engine types are `pg::…`.
- **Promote only generic constructs.** Anything that is not Chronicle-specific
  (a shader, an engine component, a system) belongs in the engine, not here.
  Chronicle mentions no engine internals it did not put there.
- **Figures** are set only in `figure-xl`, `figure` and `tick`. EB Garamond's
  default digits are lining and tabular; Cormorant's are proportional, so a
  number in a display style will reflow its row.

## Components

**Phase 2 rule: components are fed.** From `ProgressRule` on, a component exposes
*setters only* (`setPercent`, `setValue`, `setState`, …) and computes nothing from
months, stats or rules — no game logic, no reading of game state. The Life scene
listens to the engine's `WorldFactsUpdate` (`src/Engine/Systems/gamefacts.h`)
and calls the setters; a dev scene calls the same setters with fixed values. The same prefab is driven by both; the numbers
come from `rules/*.pg` through the scene, never from the component.

- **Label** (`UI/label.h`) — a style, a colour token, an alignment, and one of
  three overflows: `Grow` (box = measured width), `Wrap` (box = given width,
  text wraps, `maxLines` truncates with an ellipsis), `Ellipsis` (one line,
  shortened with `…`). Box height is always the token line height. A label is
  one entity: overflow, alignment (per line, wrapped included) and elision all
  live on the engine's `TTFText`; the label adds the style and the colour
  token.

- **Mark** (`UI/mark.h`) — one of 27 glyphs at one of five kit sizes
  (14/16/18/24/48 px), registered at exactly those sizes so nothing resamples.
  `markSizeFor(style)` is the single style → size table (body → 16, figure → 18,
  title → 24, versal → 48, the small styles → 14). An unknown name draws `seal`
  and logs once — a missing mark is visible, never blank. A mark always takes
  the colour of the text beside it.
- **MarkedLabel** (`UI/mark.h`) — a `Mark`, a `space-2` gap, and a `Label` under
  one root: the pair nearly every row is. The colour lives on the label and the
  mark takes it (structural — there is no mark-colour field). `reserveMark`
  keeps the mark column when the mark is empty so a list still aligns its text.
  A `versal` label is the one case where the mark (48 px) drives the row height.

- **Ornament** (`UI/mark.h`'s sibling `UI/ornament.h`) — the scribe's ruling and the
  illuminator's frames, in four kinds: a **Divider** (1 px `rule-hair` or 2 px
  `rule-ruled`, with an optional knot whose opaque *ground* patch masks the rule
  underneath — pass the token of the surface the divider sits on as `ground`), a
  **Flourish** band, a gold **Corner**, and a 72×72 **Versal** (an inset G4 stroke
  frame, drawn curls, and a `chapter`-set initial nudged so its *cap* — not its
  line box — centres in the frame, using `kCormorantCapRatio = 0.63`). The versal
  letter is set in `chapter` (44 px Cormorant SemiBold), the nearest registered
  atlas to the design system's 46 px versal.

- **Panel** (`UI/panel.h`) — a leaf of folio with a drawn frame, an optional
  head row (mark · title · small-caps aside on one baseline) over a knotless
  hair rule, and a `VerticalLayout` body that grows with its rows (the root's
  height is a `PosConstrain` on the body). Four frames: `Hair` (quiet), `Ruled`
  (the default), `Plain` (no ground/frame/padding), `Illuminated` (doubled gold
  frame + four inset corners, `space-5` padding — the five gold events only).
  The panel never sets a child's z: the caller builds children at
  `z ≥ spec.contentZ`, which must clear the head band (`> z + 4`). A nested panel
  is a child like any other, at `contentZ` and `contentZ + 10`. `setAside` rewrites the aside
  and re-fits the title beside it (`""` removes it for good); `setAsideColor` repaints it. There is no
  `setFrame` (an illuminated panel is a different object, not a state) and no
  shadow (separation comes from the rule).

- **Button** (`UI/button.h`) — the game's "do it" control: a face (mark · control
  label · optional "N mo" cost) on a rounded folio/`lapis`/`vermilion` ground with
  a 1px frame. Three variants: `Quiet` (default), `Study` (lapis), `Seal`
  (vermilion, irreversible — one per screen). Activation is **release inside a
  pressed face**, or Enter/Space on the keyboard-focused face — both send
  `ButtonActivatedEvent{face, tag}`. Hover tints the Quiet fill / sheens the tonal
  ones; **Tab** walks the faces drawing a 2px `focus-ink` ring 2px outside; a mouse
  click hides every ring. A disabled button dims to `opacity-locked`, swallows
  input, and **states the gap in figures** in a `caption` reason beneath it (the
  reason itself is not dimmed). There is no pressed visual and no `setFrame`.

- **Gloss** (`UI/gloss.h`) — the scribe's voice, in two forms. A **Margin** gloss
  is a 2px `rule-hair` left edge with italic `gloss` text (≤ 240px, flavour only).
  A **Tooltip** gloss is a folio leaf with a `rule-ruled` frame holding a title,
  text, rows of right-aligned tabular figures, and a caps footnote — where a row's
  real numbers live. A row may carry a **tone** that colours its value (`gain`, `loss`, `time`,
  `muted`; none or unknown is ink; the label never changes), and a row marked `heading` opens a
  **section**: a `rule-hair` rule across the gloss (`gloss.rule`), a footnote's room above it,
  then its label alone, in caps, in the figures' weight and full ink (`figure-sm`), so it stands
  over its rows; with no label it is the rule alone. An `aside` stands at the title's right, on
  its baseline (`figure-sm`, `ink-muted`), and the title elides in the room it leaves. The
  values stand in a column at the right edge, or with `inlineValues` each after its label, 8
  from it: the Life screen's glosses are inline. The Life screen's activity gloss carries everything its row does not: the
  name with how often it was done beside it (the number alone: `3`, or `0/2` for one that
  can be done twice), its group under it, a rule,
  the time and the meals, then a ruled section for each of IT BRINGS, IT TAKES and IT ASKS,
  each row under the stat's full name (`Strength  +1`), a requirement green when he has it and
  red when he is short. Its only footnote is `AT WORK NOW`, on the work he is at: what a locked
  one still asks is in red under IT ASKS, and when it closes is the tile's to say. The tooltip form is registered under a **key** on
  `GlossRegistry` and shown through the engine's `TooltipSystem`
  (`attachGloss(entity, key)`), so delay, placement, flipping and click-to-hide are
  inherited; a missing key shows a `vermilion` fallback, like a missing mark.

- **Tabs** (`UI/tabs.h`) — the book's chapters as *lettering*, not buttons: labels
  on one `rule-ruled` hairline, the open chapter marked by a 3px `vermilion`
  underline that overlaps the rule by exactly 1px, an optional count badge per tab.
  Selection is release-inside or Enter/Space; **←/→** move the selection within the
  row (no wrap). Only `TabSelectedEvent` is reported — a `Tabs` row never decides
  what a chapter shows.

- **ProgressRule** (`UI/progressrule.h`) — the quill writing across a groove: a
  `progress-track`, a `progress-ink` fill, a 45° `progress-forecast` hatch of what
  the running activity will reach (starting at the fill's head), the `quill` nib
  riding that head, and a caption of figures. Setters only (the phase-2 rule):
  `setPercent(p, animate)` tweens the fill linearly at `Motion::kMsPerPercent`
  (6 ms/point; jumps under `Motion::reduced()`), `setForecast` never animates (a
  forecast is a statement), `setCaption`/`setNib`/`setWidth` re-lay. `setGlide(p, ms)` moves
  the drawn fill to `p` in exactly `ms`, linearly, without touching the figure (`spec.percent`):
  it is how a running month fills in the month's time. `ms` 0 holds the fill where it is drawn;
  a glide never runs back, and under `Motion::reduced()` it does nothing. The fill's
  left corners are rounded 2px in the design system — not distinguishable under a
  1px frame at 8px height, so noted not implemented. The tween lives on the fill
  entity and captures the `ProgressRule` by pointer, so keep it at a stable address
  while it animates.

- **StatLine** (`UI/statline.h`) — one part of the character: *the figure is the
  point, the bar is the glance*. A name (mark S16 + `label` style, both `ink-muted`)
  at the left and the figure (`figure`, `ink`) at the right on one baseline, with the
  running activity's projection beside it (`-> 17`, in `tick`/`progress-forecast`);
  then a groove — a `ProgressRule` with the nib off and an 8px track, so the hatch,
  the fill animation and the reduced-motion rule are inherited, not repeated — solid
  fill for the present value, hatched ghost for the projected gain; a 2px
  `status-time` tick standing *on* the track (z+6, above the groove's frame) where the
  next milestone's requirement sits; and a `caption`/`ink-faint` note beneath naming
  it. Fed, not driven: `setValue(v, animate)` sets the figure at once and tweens the
  fill after it (the figure is the fact, the bar catches up) — and clears the
  projection if the value rises past it; `setProjected(p)` shows `-> p` only when
  `p > value` (a projection at or below the value is not a projection) and moves the
  figure's right anchor to the projection's left, so the figure never reflows;
  `setThreshold(t)` re-places the tick or hides it (the entity is kept — a threshold
  comes and goes as milestones pass); `setNote(s)` grows the root to follow. The
  display label is upper-cased **ASCII only** (`label` is the caps style; the four MVP
  parts — Strength, Dexterity, Intellect, Vitality — are ASCII). `glossKey` points the
  whole line at a registered gloss.

- **RequirementList** (`UI/requirementlist.h`) — prerequisites as a checked list:
  each row carries a **mark as well as a colour** (`check` in `status-gain` when met,
  `cross` in `status-loss` when not) and shows the pair "current / needed", never a
  verdict. *The mark carries the state, the label carries the words*: the mark and
  the value take the tone, the label stays `ink` when unmet and recedes to
  `ink-muted` when met — the one row in the kit where mark and label differ in
  colour by design. `met` is the scene's to say: an explicit 0/1 **wins over** the
  derived `current >= needed` (a display rule, not a game rule); non-numeric
  conditions (*"Has the Guild's letter"*) pass `met` directly and show no pair (one
  with no verdict logs once and shows unmet). Two densities: roomy (`body-sm` +
  `figure-sm`) and **dense = `tick`** for both label and value — the mark stays S14
  in both. Values right-align so the slashes stack; the label's ellipsis width
  leaves room for the pair. Fed: `setItems` rebuilds, `setItem(i, c, n)` updates a
  pair in place (re-deriving `met` unless explicit), `setMet`/`clearMet` repaint
  tokens only — nothing moves.

- **LifeClock** (`UI/lifeclock.h`) - one whole life on a single track, the spine of
  the Life screen (which feeds `setUnit` his age to the month, `YEARS 10 MONTHS OLD`, and shows
  no next milestone). The head line: the age in `figure-xl` with `YEARS` beside it on
  one baseline, and at the right the next milestone with the `time` mark and the
  months to it. The track (14 px, framed): months lived solid, the running activity
  hatched ahead of them with a 1 px hairline at its start, age-limited windows as a
  **wash between two ruled edges** (never a solid band: the lived fill stays readable
  through it; a closed window keeps its wash and turns its edges `rule-ruled`), and
  ticks at the milestone ages only, labelled below and nudged inward at the ends.
  **The promise**: the clock advances by exactly the months the activity promised -
  a running segment of *n* months, once settled by `setRunning(0)` and
  `setAge(age + n / 12)`, ends where the lived fill then ends. `xFor(age)` is the
  root-relative x of an age on the track's inner span (pure; the tests and the scene
  place things with it). `setAge(a, animate)` changes the head figure at once and
  tweens the fill at 600 ms per whole span (a month is about 1.4 ms: the clock
  ticks, it does not sweep), the hatch riding the fill's head; `setRunning`,
  `setNext`, `setWindows`, `setWindowClosed` and `setMilestones` are never animated.
  Elements: `clock.age`, `clock.unit`, `clock.next`, `clock.next.mark`,
  `clock.next.in`, `clock.track`, `clock.frame`, `clock.lived`, `clock.running`,
  `clock.running.edge`, `clock.window.wash`, `clock.window.edge` (`.closed`),
  `clock.tick.mark`, `clock.tick.label` (`.past`).

- **ActivityRow** (`UI/activityrow.h`) - the game's main verb: one thing the character
  could spend months doing, in four states. Idle shows the gains (`tick`, `"STR +1   VIT
  -1"` with a real minus; they wrap in the room between the mark and the cost, a gain never
  parted from its figure, and the row grows by the lines they take); running draws a verdigris edge and mark and a `ProgressRule`
  whose caption names what the activity reaches at term; locked keeps the row fully
  legible in `state-locked` and states what it still needs as a dense
  `RequirementList` in place; selected is a 3 px `focus-ink` edge inside the row's left.
  The month cost (`time` mark + `control` figure) shows in every state, right-aligned so
  the figures share an edge down a list. One entity per part and state changes are
  element swaps (`activity.row.ground` is transparent at rest); only the middle block
  (gains, rule or list) is rebuilt by `setState`, and the list re-stacks on the new
  height. The row also lives on its root as a component, and every setter keeps that
  copy and the caller's in step. `setName` elides the name only when it no longer fits
  between the mark and the cost. The mark is S24 (the design system draws 22; the kit
  registers 18 and 24). `count` (`"DONE 2 · 1 LEFT"`) is how often it was done and what is
  left of a limited one: it follows the rank on the rank's label (`"RANK 2 · DONE 2"`), and
  `setCount` rewrites it in place on a row built with a rank or a count. A running row may glide
  (`setGlide`, or `glideTo`/`glideMs` in its spec, started on the live row once it is attached);
  `setPercent` ends a glide. `until` (`"CLOSES IN 14 MO"`) is when it closes: a `tick` line of
  its own under the middle block, in every state, 2 px apart; the row grows by it (16 + 2), an
  `urgent` one is written in the loss's colour (`activity.until.urgent`), and `setUntil("")`
  removes the line. A **compact** row (`compact`, or every row of a list with `compact: true`)
  is its name, its time and its closing, and nothing else: no rank or count, no `each`, no gains,
  no rule and no list of what it asks, in any state (48 px, 66 with a closing). Its gloss says
  the rest, and its ground says what kind of thing it is, three at rest: `activity.kind.timed`
  (work that takes months, a lapis wash), `activity.kind.instant` (a thing done at once, a
  transaction: ochre) and `activity.kind.locked` (what he cannot do yet: ink, faint), with
  `activity.kind.running` for the work he is at. Hover deepens the kind's own ground (`.hover`).
  The setters still keep what a compact row does not show (`setCount`, `setPercent`,
  `setRequirement`), for whoever says it. A **tile** (`tile`, a compact row stacked for a grid)
  has no mark: its name is set in `control` (`activity.tile.name`) at its left edge, wrapped in
  its width over two lines at most, then elided; the time stands under the name, the closing
  under the time (padding 8, 58 px with a one-line name). `natural` is the height its content
  asks and `minHeight` what its list asks of it: `setMinHeight` keeps a line's tiles level.

- **Tiles in a list** (`ActivityListSpec::tileWidth`, `tileWidth:` in a file) - with a tile
  width the rows a list is given (`setRows`) are tiles, as many to a line as fit at that width
  or more (`columns()`), sharing the list's width 8 apart (`tileSize()`), a new line with every
  group. A line is a prefab in the list's body (`ActivityLineState`) its tiles are anchored on,
  so the list's clip and scroll reach them through it, and the list finds its rows through it.
  Every frame the system levels each line (`ActivitySystem::level`): its tiles are as tall as
  the tallest asks, and the line holds them with the 8 to the next, so a closing that comes or
  goes moves what is under it and nothing else. Another width lays the tiles again
  (`setSize`): each as it stands (state, closing), the selection let go. Rows that are children
  of the list in a file are not tiles.

- **ActivityList** (`UI/activityrow.h`) - the rows in groups, headed in the display face
  (`activity.group`, 24 px, `space-3` above all but the first, `space-1` below), with
  the grounds alternating across the whole list like an account book, not per group,
  and the last row's rule hidden. **Selection is the list's**: at most one row, set by a
  release on it or `select(id)`; locked and running rows never select (a click does
  nothing, `select` logs). **Confirm** is a second release on the selected row within
  400 ms, or Enter / Space with keyboard focus on it, and sends
  `ActivityActivatedEvent{list, id}`; Enter on another row selects it first. Tab skips
  locked rows and the focus ring sits 2 px inside the row. The list finds its rows by
  walking its body each frame (`ActivitySystem::execute`), so a list filled by the
  prefab builder behaves like one built by `makeActivityList`. In a file the list is
  **flat**: `ActivityGroup` (a heading) and `ActivityRow` children side by side, the rows
  after a heading being its rows; the body stacks one level only. Starting the activity is the scene's business. The list
  is Panel-like: a root prefab (anchor it, name it) and a `body` layout the rows stack
  in. With `height: 0` it is as tall as its rows; given a height it keeps it, clips its
  rows to the body and **scrolls** (mouse wheel, a thumb on the right edge,
  `activity.scroll`, and a press dragged 4 px or more on the rows); rows out of view take no
  hover and no click. The release of a drag reaches the list cancelled
  (`OnMouseRelease::cancelled`) and selects nothing; Button and Tabs ignore a cancelled release
  too.

- **WindowMeter** (`UI/windowmeter.h`) - a door that closes: an age-limited chance, how
  much of it is spent, and what still fits inside it. The head: the `gate` mark (S16) and
  the name in `tab` at the left, the age range (`"16–22"`, en dash, `control`,
  `ink-muted`) at the right on the name's baseline; the name elides in the room the range
  leaves. Under it a small `ProgressRule` (6 px, no nib) filled by
  `(age − from) / (to − from)`, clamped, and the note that does the work (`tick`, wrapped
  at the meter's width; a second line grows the root, 38 + note height). **The note is in
  attempts, never percentages** (*"Open 22 more months. One attempt fits; two do not."*),
  and it is the scene's string: `windows.pg` counts the attempts from the activity's
  months. **Three states, each an element suffix** on the name, the mark and the note:
  upcoming (`.upcoming`: ink name, verdigris note naming the entry requirement), open (no
  suffix: ochre name and mark, `status-time` note) and closed (`.closed`: all
  `state-locked`, the mark a **`cross`**, back to `gate` when it leaves Closed). A closed
  meter stays fully legible and on the screen for the rest of the life. The meter never
  decides its own state: `setAge`, `setState`, `setNote` and `setRange` are fed by the
  scene. Elements: `window.name`, `window.mark`, `window.note` (each `.upcoming`,
  `.closed`), `window.range`.

- **ResourceLedger** (`UI/resourceledger.h`) - everything the character holds, as a column
  of account-book rows grouped by kind (PURSE, STANDING, STORES, KEPT BETWEEN LIVES). A row
  is mark (S16) · name (`body-sm`, never wrapped) · a **dotted leader** · figure
  (`figure-sm`) · rate (`tick`, `status-gain`; a leading minus paints it `status-loss`),
  26 px apart; a group heading is the `label` style in `ink-muted`, `space-3` above all but
  the first and `space-1` below. **Strings in, no arithmetic**: the value and the rate are
  the scene's, already formatted (`"412"`, `"−3"`, `"2 of 3"`, `"+2 / mo"`). **The leader
  is engine-sized**: a `DottedLine2DObject` anchored from the name's right + 8 to the
  figure's left − 8, 3 px above the baseline, so `setValue` moves only the figure's left
  edge and the leader gives way; every figure is right-anchored to the row (or to its
  rate), so the figures share one right edge down the ledger. A name that leaves under
  24 px of leader is a content error and logs one warning. Tones colour the mark (`coin`,
  `guild` ochre; `relic` gold-edge, figure too); `muted` greys the mark, name and figure,
  and **state beats tone**: a muted relic is `ledger.figure.muted`, not gold. **A row never
  earned this life is never added**: the scene adds it (`addRow`, in the order of first
  earning; an unknown group is appended with the label given) and may `removeRow` it.
  Fed: `setValue`, `setRate` (`""` removes), `setMuted`. In a file the groups hold rows,
  so they are **child kinds**: `ResourceLedger > LedgerGroup (id, label) > LedgerRow`
  (`id`, `glyph`, `label`, `value`, `rate`, `tone`, `muted`, `glossKey`). Neither builds
  anything of its own; each adds to the ledger it is nested in. Elements: `ledger.group`,
  `ledger.mark` (`.coin`, `.guild`, `.relic`, `.muted`), `ledger.name` (`.muted`),
  `ledger.leader`, `ledger.figure` (`.relic`, `.muted`), `ledger.rate` (`.loss`). A row whose
  value is `""` is its name alone: no figure and no leader.

- **EventLog** (`UI/eventlog.h`) - the running account of everything that has happened in
  this life, entered under the year it happened in: a `vellum-worn` well with a 2 px
  `rule-hair` left edge, and inside it (inset 8 / 12) a scrollable `VerticalLayout` of lines
  one pixel apart. A row is age (`caption`, a 30 px column, written year.month with the month
  1 to 12: `14.4`, `16.12`, then `17.1`) · mark (S14) · text
  (`body-sm`, wrapped in the room the figure leaves: a long line is read whole and its row grows
  by the lines it takes) · figure (`figure-sm`, right edge), on the baseline of the text's first
  line. A figure wider than half the line (many things gained at once) goes **under** the text
  instead, wrapped and right-aligned on the same edge; the rules join an amount to its stat with
  a no-break space (`lib.pg`'s `noBreak`), so it breaks between two gains, never inside one. **Kinds → glyph / element**: note → `quill`, `log.text.note` (`gloss`, italic,
  muted: what happened *to* him); gain → `check`, loss → `cross` (mark, text and figure in
  `status-gain` / `status-loss`); coin → `gold` (ochre mark and figure, plain text);
  milestone → `seal` in gold-edge, the text in `figure-sm` for full ink and weight. An
  entry's own `glyph` wins; the element stays the kind's. **Rubrics**: the log inserts one
  when the whole year passes the last - grouping what it is given, not game logic -
  `yearPrefix` + ordinal + ` YEAR` (`IN HIS 14TH YEAR`; `1ST 2ND 3RD 4TH … 11TH 12TH 13TH
  21ST`), 12 px above all but the first. The words are `gloss-title` vermilion (the design's
  15 px display size does not exist in the kit); the ordinal is a label of its own in
  `figure` (`log.year.ordinal`, same size and line): Cormorant's old-style figures sat below
  its capitals, the text face's are lining. The three share a baseline, a space apart. **The whole life is kept, never truncated**: rows
  scrolled out of the well are clipped (`ClippedTo` the list), not removed. **Stick-to-end
  rule**: `append` sets the layout's `stickToEnd` to `atEnd()` for that insertion, so the view
  follows new lines only when it was already at the end; a player reading further up is left
  where they are (`atEnd` = offset ≥ content − viewport − 1; `scrollToEnd` jumps there).
  Wheel scrolling is the engine's `layoutScroll`. The list runs 8 px into the right padding
  for its **thumb** (`log.scroll`, 4 px, `ink-faint` at 45 %): the layout sizes it to what is
  in view, hides it while everything fits, and dragging it scrolls the list. The rows themselves
  drag too (the layout's `dragToScroll`): a press and a 4 px move scroll the life like the
  wheel. A new line and
  its parts are born unobserved and drawn only once the layout has placed the line in view,
  so nothing flashes where it was made. A layout holds its children at its own z,
  so the lines share the list's band; their parts stand two and three above it. The entity
  count grows with the life - a 36-year life at ~30 entries a year is ~1 100 rows, ~4 400
  entities: fine for the MVP; the answer to a bad profile is row virtualisation, not
  truncation. Fed: `append`, `clear`, `setFootnote` (`caption`, `ink-faint`, 4 px under the
  well). Elements: `log.gutter`, `log.edge`, `log.year`, `log.age`, `log.mark` (`.gain`,
  `.loss`, `.coin`, `.milestone`), `log.text` (`.note`, `.gain`, `.loss`, `.milestone`),
  `log.figure` (`.gain`, `.loss`, `.coin`), `log.footnote`.

## Game rules (`rules/*.pg`)

**Balance lives here.** Every number the Life screen shows about the future or the rules - what
an activity costs and gives, what a milestone asks, which doors are open and how many attempts
fit, what a stat will be at term - comes from `examples/Chronicle/rules/*.pg`, never from C++.
C++ owns the clock, the save, the scene and the events; the scripts own the content and the
evaluation, so a rule change or a mod is a `.pg` edit with a test, not a rebuild. The numbers
are the 7-30 alpha balance of `res/chronicle/chronicle-balance` (`chronicle-nine-lives.md`,
`tasks.json`): fifty activities on three paths (the Keep, the Collegium, the Hidden Hand), with
Vitality mending kept on top of it.

**The contract** (`Core/rulevm.h`, the prefab loader's seam): *inputs are globals the C++ side
defines, outputs are globals the script leaves behind*, read back by a dotted path
(`forecast.atTerm`, `activities.3.gains`) with the engine's `vmread` readers
(`src/Engine/Compiler/vmreaders.h`). A `RuleScript` compiles once through the ECS's script
registry and re-runs its whole top level from the prepared bytecode
(`prepareCachedFunction` / `runPreparedFunction`): there is no `main()`, and a script keeps
nothing between runs because each run redefines everything it leaves.

- **No `nil`**: every optional output is present, as `""` or `-1`. A missing output global or
  field is an error when read, never a default. (In PgScript a missing table key reads as
  `false`, so `lib.pg`'s `lookup` is how scripts read a key that may be absent.)
- **No `ecs`, no events**: scripts import `string`, `math`, `algorithm` and `lib` only
  (native modules are imported per script: `lib`'s imports do not carry).
- **Errors are data**: a script refuses its inputs by pushing onto its `errors` list, or, for
  the forecast, by setting `forecast.error`; `run()` fails and logs each message with the
  script's path. A compile error fails `load()`, with the VM's message in the log.
- Language notes: `from` is a keyword (`w["from"]`), a one-line `{ ... }` block needs a newline
  before `}`, numbers print as `std::to_string` does (`17.500000`), so text is worded with ints.
- **Every function lives in `lib.pg`, ahead of its tables.** A function is a constant of the
  script that declares it, and its index must fit in a byte (`Chunk::addConstantIndex`): one
  declared after the 256th constant does not compile, and an import is compiled into the
  importing script, so a `fun` after `import "activitytable"` (hundreds of strings) is past it.
  The functions read the tables by name when they are called.

**Loaded scripts are never imported.** Loading a script writes its bytecode beside it (`x.pgc`),
and an `import "x"` that finds an `x.pgc` takes it instead of the source. That path does not
compile here (`windows.pg` importing a compiled `activities` fails in the compiler's
`PoppingJumpPass`), and it would not follow an edit of `x.pg` either. So `activities.pg` and
`milestones.pg`, which the game loads, only import `activitytable.pg` and `milestonetable.pg`,
which hold everything and are what `windows.pg` and `forecast.pg` import. `lib.pg` is never loaded.

| script | inputs | outputs |
|---|---|---|
| `lib.pg` | - | pure helpers: `clamp`, `ordinal` ("14th"), `monthOf` (whole months lived), `monthsBetween`, `monthsToAttempts`, `signed` ("+1" / "−2"), `fmtMonths` ("6 mo"), `numberWord`, `statName`, `statOf`, `lookup`, `raised` (a gain stopped at the stat's ceiling), `byId`, `pathFlagOf`, `asksOf`, `openNote`, `shiftIn` (a month's change to a stat); `lastAge` (30, where his prime ends), `anyAge` (the `finishBy` of what no age closes) |
| `activities.pg` (`activitytable.pg`) | `age`, `character`, `done`, `activityId` (`""`: all; `forecast.pg` shapes only its own), `world` | `activities`: `{id, group, name, glyph, months, rank, each, path, enters, board, fromAge, finishBy, gains[{stat, amount, label, name}], costs[{stat, amount, label, name}], requires[{stat, label, current, needed}], locked, done, uses, left, spent, closed, pathOpen, listed, showAge, showFrom, tally ("DONE 2", "DONE 0 · 1 LEFT"), until ("" or "CLOSES IN 14 MO"), urgent, after[step]}` |
| `milestones.pg` (`milestonetable.pg`) | `age`, `world` | `milestones`: `{age, id, label, passed, entry, asks[{path, stat, label, needed}]}`; `next`: `{id, label, age, in}` (`in` = months to it; `"", "", -1, -1` past the last) |
| `windows.pg` | `age`, `character`, `done`, `activityId` (`""`) | `windows`: `{id, name, from, to, state (upcoming / open / closed), note (in attempts), attempts}`, one per door of an activity whose path is open to him |
| `forecast.pg` | `age`, `character`, `done`, `activityId`, `monthsIn` | `forecast`: `{atStart{stats}, atTerm{stats}, percent, months, caption ("MONTH 3 OF 6 · STRENGTH 14 → 16 AT TERM"), gaps[{stat, label, current, needed}], entries[{text, kind, figure, glyph}], error}` |
| `resources.pg` | `age`, `character`, `board` | `month`: `{after{stats}, entries[{text, kind, figure, glyph}], hurt[stat]}`; `rows`: `{id, group, groupLabel, glyph, name, tone, rate ("+2 / mo"), limit}`; `glosses`: `{id, title, text, footnote, rows[{label, value, tone}]}`; `caps`: `{stat, most}`; `death`: `""` or the line the next life opens with |
| `achievements.pg` | - | `achievements`: `{id, name, entry, asks[{fact, op, value}], gives[{stat, amount}]}` |
| `epitaph.pg` | `age`, `character`, `done`, `activityId` (`""`), `deeds` (the names of the deeds reached) | `epitaph`: `{cause ("He died an old man, in his thirty-third year."), story[text] (what he became, worked at, left, and what is told of him), text (the story as one paragraph), tally ("AGE 33 · WORKS 14 · COIN 31 · DEEDS 2")}` |

**Repetition** (`activities.pg`). `done` is the save's count of terms completed per activity.
An activity may carry two optional fields:

- `uses: N`: it can be done N times. `left` counts down and `spent` turns true at the last
  one; the Life screen drops a spent activity from its list.
- `after: [{uses: N, ...}]`: what repetition changes. `rank`, `months` and `gains` in a step
  replace the activity's own once N terms are done (an upgrade). `effects [{stat, amount}]`
  and `entry` happen once, at the term that makes it N (a trigger): `forecast.pg` adds the
  effects to `atTerm` and the entry to `entries`.

**Costs and things done at once** (`activities.pg`). `costs: [{stat, amount}]` is what an
activity takes: it is listed in `requires` (he must hold that much, or the row is locked) and is
taken **the moment it begins**: `forecast.atStart` is the character once it is paid, and the
scene takes it on confirm; `atTerm` does not take it again. The row shows each cost as a
negative figure beside the gains. `months: 0` is an activity done at once: confirming it applies
`atTerm` (its gains on what is left once it is paid) immediately, no month passes, and it can be
done while another activity runs. Its row reads `NOW` where the others read their months.

**Ages and paths** (`activities.pg`). `fromAge` (default 7) is the youngest he may begin it:
younger, the first thing it asks is the age. `finishBy` (default `lastAge`) is the latest its
term may end, inclusive: an 18-month activity with `finishBy: 20` is begun by 18.5. Past that
it is `closed` (`anyAge` for what no age closes). A closing is said ahead: `until` is the
world's date of the last month he may begin it, month/year (`"CLOSES 5/11"`, or `"CLOSES THIS
MONTH"`), said from `closingShown` (24) months before it, `urgent` from `closingUrgent` (6).
The date does not move as the months pass; the scene writes it on the row
(`activity.<id>.until`, `.urgent`) every month, so no row leaves the list unannounced, his
prime's at 30 included. `path` puts an activity on one of `paths` (`warrior`, `mage`, `thief`, each with
the flag that makes him one of it); the one that `enters` it is open until he is one of any
path, the others only once he is one of theirs (`pathOpen`). The paths exclude each other. A row
is `listed` while its path is open, it is neither spent nor closed, he is of age for it, and
he has most of what it asks: `reach` is the average share (each up to 100) of its requirements
he meets, costs and room left out, and the row shows from `reachShown` (75). An activity may set
its own two: `showFrom` (the share that lists it, 0 for whatever he has) and `showAge` (the age
its row is listed from, before he may begin it: it stands locked, the age the first thing it
asks). The great step of each age carries both, so it shows from the milestone before as what
the years he is in lead to: the ways into a path from 13, a path's proving from 16, its mastery
from 21. A boy is not shown
a life of locked rows; the doors on the clock still say what is coming. The scene shows the
listed rows only, and rebuilds the list whenever what is listed, or what a row asks, changes. A requirement
may be `eased: {flag, needed}` (with the flag it asks less) or `anyOf: [flags]` (any one will
do); `bonus: {stat, needed, gains}` replaces the gains when the stat is high enough. Every age
test counts whole months (`monthOf`), so an age summed a twelfth at a time meets its doors on
time.

**Meals** (`activities.pg`, `resources.pg`). `board: true` is work that feeds him: a month at it
takes no ration and costs no Vitality, held rations or not. The scene runs `resources.pg` with
`board` for the activity at work. Eating the last ration is still a fed month: only a month
begun with none costs Vitality.

**Ceilings** (`lib.pg`). `statCeilings` is the most a stat can ever be: parts 30, skills 10;
renown, favors and what he holds have none. A term's gain stops there (`raised`), what already
stood past it stays, and nothing is locked by it.

**Holdings** (`resources.pg`). A holding is a stat with a row in the ledger: coin, rations and
reagents in the purse, favors and the paths' standings, and under TIES (`ties`) every flag a
work may leave him (`mara_student` "Mara's pupil", `watch_known` "Known to the Watch"...), so
what a term's log line says he gained is then on the page. A standing, a title or a tie is a
`title: true` holding: he has it or he has not, so its row shows its name alone (no figure, and
the ledger draws no leader to an empty one) and its gloss counts nothing and states no limit. `produces
[{stat, amount, per}]` (no holding uses it in this balance) brings `amount` of `stat` every month for each `per` of it he holds;
`decays: N` takes N from it every month, never below 0; `empty {effects, entry}` says what
running out costs: the entry the month it reaches 0, the effects every month it stays there.
The script runs once per month for `month.after`, and once per publish for the rows' rates.
`about` is a sentence on what the holding is. Each holding leaves a **gloss** (`glosses`): what
he holds (over its limit when it has one, on one row: `3/60`), what the month does to it and where that comes from, what it brings
(a holding that `produces` is footnoted as a generator), what it uses and how long it lasts,
and what running out costs. The Life screen registers it as `resource/<id>` on the ledger row;
a row the rules do not follow says only what he holds.

**Limits** (`lib.pg`). `statLimits` is the most he can hold of a stat (`limitOf`, 0 for none;
`roomFor` is what he can still take in). What a month produces stops at the limit
(`resources.pg`). An activity that brings such a stat asks for room (`activities.pg`: one more
requirement, `Room for Rations`, current = the room left, needed 1), so it is locked while he
is full. Begun with any room left it brings all it brings: **an activity may carry him past the
limit**, and what he holds past it is only eaten into, never cut.

**Caps and mending** (`lib.pg`, `resources.pg`). `statCaps` names the stat holding the most
another can be (`vit` -> `vitmax`). What takes from the stat leaves the cap alone; a gain from
an activity raises both (`forecast.pg`). `mending [{stat, amount, every}]` brings the stat back
toward its cap, `amount` every `every` months counted in `<stat>rest`; a month that took from it
mends nothing and starts the count again. `caps` (`[{stat, most}]`) is what the scene shows as
the part's ghost when no activity is chosen, and as `At most` in its gloss. A save from before
the cap takes what it has as its most.

**The end of a life** (`resources.pg`). `mortal` lists the stats a life cannot go on without.
`month.hurt` lists the ones the month took from: the scene turns those parts red for a moment
(`StatLine::setAlert`, through `character.parts.<p>.alert`) and pauses the months at the first
month of a run of them.
`death` is the entry of the first one the character as given has none left of, else `""`. The
scene asks after every month, **before the term of the activity at work pays**, and after every
activity done at once; on a death it pauses the months and shows the ending, and the life that
begins from it writes the entry in its log, with the age the last life ended at (`mortal`'s
`aged` entry when it ended past `lastAge`).

**The ending** (`epitaph.pg`, `res/chronicle/ui/ending.yaml`). When a life ends the scene runs
`epitaph.pg` on the character as he died and builds `ending.yaml` over the page, in the Overlay
band: a veil over the whole window (`ending.veil`) and an illuminated leaf in its middle with his
name (the save's), `cause`, `text` and `tally`, and one Seal button. The page under it is
published once more, so it shows the life as it ended. The veil takes the mouse (hover is
topmost-only, so no row or button under it is hovered or clicked), and the scene refuses months,
confirms and `SPACE` while `ended`. *Begin a new life* (tag `life.again`) or `N` removes the
ending and starts the next life. A save written at an ending opens on it. If the ending cannot
be built, the next life begins at once, as it did before there was one. The words are table
driven in the script: `standings` (the highest flag he holds is the one said), `legacies` (by the
coin he held), the work he went back to most (terms of timed activities, the table's order on a
tie) and the deeds by name.

**Old age** (`lib.pg`, `resources.pg`, `activities.pg`). The last milestone (`lastAge`, 30) ends
his prime, not his life: its `entry` is written and the months go on. Every activity closes there
unless it says otherwise (`finishBy` defaults to `lastAge`); the *Old age* group and the market's
rations carry `finishBy: anyAge` and stay. The group is listed from `lastAge - 1`: the last
months of his prime are too short for any other work, and the months only run while he works.
`aging [{stat, fromAge, amount, every}]` takes `amount` from the stat's cap every `every` months
past `fromAge`, and from the stat when it stood above: nothing mends it back, so a life always
ends. It is not in `month.hurt`: no red, no pause. The clock's track stops at 30; its age
figure does not.

**Deeds** (`achievements.pg`). Each deed asks facts the Life scene publishes (`stat.<key>`,
`done.<activity id>`, `life.age`) with an `op` of `>=`, `>`, `<=`, `<` or `==`. The scene hands
them to the engine's `AchievementSys` (`src/Engine/Systems/achievement.h`); when one is
reached the scene applies `gives`, writes `entry` in the log and keeps the id in the save.

`Rules` loads them once from a root (`examples/Chronicle/rules` from the repo root, where
the game runs; `rules` beside `test_chronicle`, where CMake copies them) and gives each a typed
call. The forecast runs once per month tick, not per frame; `rules_test.timing_ceiling` keeps
100 runs under 100 ms (each run rebuilds the whole activity table: about 0.55 ms). The compiled `.pgc` files land beside the scripts and are git-ignored.

## Patterns

- **State component + System + event-driven tests.** A stateful, input-receiving
  component (Button, and the tabs and every phase-2 row after it) puts a
  `chronicle::…State` `pg::Component` on the hit entity, a matching `…System` that
  `Listener`s the engine's input events (`HoverChangedEvent`, `OnMouseClick`,
  `OnFocus`, …) and repaints via an `applyVisual`, and tests that drive those
  events (`OnMouseMove`/`OnMouseClick`/`OnSDLScanCode`) exactly as `test/hover.cc`
  does — `pump()` = 3×`executeOnce` after each.

- **One Tab order for the kit (`FocusOrderSystem`).** Keyboard focus traversal is
  not each component's job: `FocusOrderSystem` owns one order for every focusable
  face (buttons, tabs, later rows), walks it on Tab/Shift-Tab (skipping disabled),
  and announces every change with `KeyboardFocusChangedEvent`. A component's system
  only draws its ring from that event and calls `add`/`setEnabled`/`focus(id)`.

## Phase 1 complete

**2026-09-21** — Phase 1 lands the kit's seven components and seven dev scenes,
all tested (133 `test_chronicle` + engine `t1` green):

- **Components**: `Label`, `Mark`, `Ornament`, `Panel`, `Button`, `Gloss`, `Tabs`
  (plus the engine's `ThemeSystem` and the `FocusOrderSystem` service).
- **Scenes**: `TypeSpecimen`, `LabelGallery`, `MarkGallery`, `OrnamentGallery`,
  `PanelGallery`, `ButtonGallery`, `TabsGlossGallery`.
- **Patterns**: the state-component + system + event-driven-tests shape, and one
  Tab order for the whole kit. Phase 2 (`ProgressRule` first) builds on these.

## The Life scene

`Scenes/lifescene.h`. The page is `res/chronicle/ui/life.yaml`, built as it is: a head of
two lines (the title in the heading face, then what he is; the world's date at the top right,
its month and season under its year: his age is the clock's to say) and three columns of panels from 70 down, 12 apart, 16 from the window's edges. Left
(300): what he holds, then what he has learned:
his parts (Strength, Dexterity, Intelligence) over his skills, in one panel. Middle (604, the rest of the width): the choice alone, with no heading: the tabs (the book's
chapters) stand at its head, over a grid of tiles
(`compact: true`, `tileWidth: 148`): three to a line at 1320, more as the window widens, each
a name, a time and a closing on the ground of its kind (work that takes months, a thing done
at once, what he cannot do yet), everything else in its gloss. The running tile draws no rule
there: the rule is *At work now*'s. Right
(360): the years (the clock, with his Vitality under his age), the work at hand, and the log,
which takes the rest of the column down to the bottom. The scene does the
three things a file cannot. In `startUp` it runs the rules and the save and fills the
rows only they know: the ledgers, the activity lists, the log, the clock's ticks and bands.
In `wire()`, the one function with every subscription, it subscribes each widget to a
`WorldFacts` path through the `FactRouter` (`Core/factrouter.h`). And it turns input into
game events: select (the forecast's ghosts on the parts), confirm (the activity at work,
refused while one runs), a month passes. It computes nothing about the game: every number
it writes is a script's output or the save's (`lifescene_test.no_scene_arithmetic` scans
the source for it). A month: the age moves a twelfth, `resources.pg` is run (fed or not by the work at
hand) and its `month.after` becomes the character, a life with nothing left ends there, then
the running activity's forecast is re-run and written, and at term its `atTerm` becomes the
character, its lines the log's, and the milestones, doors and locks are run again. The term of
a path's way in sets the save's `aim`, the path whose asks stand on his parts. A term ended also counts in the save's `done`, and the activity rows
are rebuilt so a spent one leaves and an upgraded one shows what it has become. `onLeave`
drops every subscription and the page leaves with the scene. The save (`Scenes/lifesave.h`,
`save/chronicle/life.sz`) holds the character, the life, what he holds, the log, the terms
done, the deeds reached and the world's month (`world`); nothing a script outputs is saved.

**The world's calendar.** `LifeSave::world` counts the months since the world's Year 0. It
moves with every month that passes and a new life keeps the last one's count (`newLife`): a
death does not rewind it, so it says how long the chronicle has been kept. The scene hands it
to the rules (`Rules::world`, the `world` input of every script that reads the two tables), which
write the dates: the head's (`headline.date` "YEAR 10", `headline.dateNote` "MONTH 7 · SUMMER ·
BELLMOOR") and each activity's closing. A fresh life started with `--fresh` begins at Year 0;
the mockup's stands at month 126.

**The three columns at every size.** The three columns are the page down to a window of
680 x 566. They come in three widths and three heights (`Widths`, `Heights` in `lifescene.cpp`),
and a window gets the roomiest step it holds of each:

| width from | left | right | margin | gap | | height from | between panels | least log |
|---|---|---|---|---|---|---|---|---|
| 1136 | 300 | 360 | 16 | 12 | | 664 | 16 | 120 |
| 916 | 248 | 296 | 10 | 8 | | 610 | 10 | 84 |
| 680 | 200 | 280 | 6 | 4 | | 566 | 4 | 56 |

`life.yaml` is drawn at the first step. For another the scene writes the widths, margins and gaps
into the page before it is built (`shapeColumns`), so a step is the page built again, as the
compact page is; what stands in a panel takes its width from the panel. In the narrowed columns
the row at work keeps its name and its time and leaves the rest to its gloss, and at the
narrowest the log has no footnote and a pause reads `PAUSED`.

**The left column scrolls.** It has no least height: it grows with what he holds and has
learned. Its two panels stand in one scrolling layout (`left` in `life.yaml`, at the panels'
depth: a layout gives its own to what it holds), as tall as the window leaves it. When they are
taller the wheel moves them, and a thumb at the column's right edge, which the scene makes when
it builds the page, shows where and can be dragged.

**The compact page.** Under the three columns' least size (680 x 566) the page is
`res/chronicle/ui/life-compact.yaml`: the work at hand over the choice, and a side column of one
panel at a time (Stats, Inventory, Timeline, Log). It is the fallback of a very small window
and nothing else: what a life holds never sends the page there.

z on the page: page 0, panels 10, their content 20–59, tooltips 200.

## Gate: phase 2

- **The mockup** (2026-09-30, `--no-save` at 1320 × 1020): the head (title, subtitle and age
  from the scripts), the tabs, the three columns in order with nothing overlapping and the
  page inside 1020, the clock's six ticks and five bands, two doors, the ledger's leaders,
  the stat lines with the Squire's threshold on Strength, the activity list with its locked
  Squire row, the log's rubrics and the skills all read from the save and the rules. Not
  compared side by side with the design system's MainLifeScreen yet: that comparison, and
  any component fix it asks for, is the first open item of the gate.
- **A fresh life** (`--fresh`): age 7.0, WINTER, 84 months to the fourteenth; the ledger
  empty, the log one rubric and "Childhood", every door upcoming with what it asks. The
  months running on `SPACE`, and the entity count over ten months, are still to be
  watched by hand (the month loop itself is covered by `lifescene_test`).
- **A saved life**: the save round trip is covered by `lifescene_test.save_round_trip`;
  `S` at 17.9, quit, run again, and a *Train at the yard* run to term (the log's last
  line, the stat, the clock's edge, *At work now* emptying) are still to be checked by
  hand, with `T` in the middle of the run.

## Gate: phase 1

- **Passed** (2026-09-20): `--dev PanelGallery` compared against the design
  system's Panel preview. Rule weight, padding, heading (mark · title · aside on
  one baseline), the knotless divider, the wrapping body, and the illuminated
  corners all read the same; the frame/rules/ink/gold switch cleanly in Candle
  with nothing moving. No 1.1–1.4 fix was needed. Phase 1 is closed.

## Assets

- The **mark** set (`res/icons/chronicle/`, 27 glyphs) and the **ornament** set
  (`res/icons/chronicle-ornaments/`, 7 curves) are SVGs authored on **square**
  viewBoxes: `SvgLoader::rasterize` fits a document into a square and centres it,
  so a square viewBox makes the quad and the drawing coincide with no offset. The
  four corners ship pre-mirrored (`corner-tl/tr/bl/br`) because the icon system
  has no flip.

- The **tokens** live in two files that are always identical:
  `res/chronicle/tokens.json` (the game) and `testdeps/chronicle/tokens.json` (the
  tests). Edit the first, copy it over the second, commit both together. The token
  values (`name`, `version`, `color`, `spacing`, `radius`, `border`, `opacity`,
  `shadow`, `type.groups`) belong to the design system; `type.fonts`,
  `type.families` and `elements` belong to the engine. `tools/merge_tokens.pg`
  brings a new design-system export in without touching the engine's part:

  ```bash
  build/PgCompilerBootstrap tools/merge_tokens.pg <design-system tokens.json> res/chronicle/tokens.json
  cp res/chronicle/tokens.json testdeps/chronicle/tokens.json
  ```

  It splices the raw text, so an unchanged export is a no-op diff.

## Adding a dev scene

One line in `Scenes/devscenes.cpp`: add `{"MyScene", [](auto* s, auto* t, auto* st){ s->loadSystemScene<MyScene>(t, st); }}`
to `devScenes()`. `--dev` and its error listing pick it up automatically.

## z-layer bands (all `int`)

| Band          | z range |
| ------------- | ------- |
| Page          | 0–9     |
| Panels        | 10–19   |
| Panel content | 20–59   |
| Banners       | 60–69   |
| Overlay       | 100–129 |
| Tooltip       | 200–209 |
| Debug         | 900–909 |

