# Chronicle

The Chronicle UI kit, built on ColumbaEngine. Everything here lives in the
`chronicle` namespace; engine types are used as `pg::…`.

## Running

Run from the repo root (assets are read relative to the working directory):

```
./release/Chronicle                           # the Life scene, from save/chronicle/life.sz (the mockup's life when there is none)
./release/Chronicle --no-save                 # the mockup's life, never written
./release/Chronicle --fresh                   # a first life: six years old, one task, the guide
./release/Chronicle --fresh --guide 6         # the same, its guide already past step 6
./release/Chronicle --fresh --age 7           # a boy of seven, his sixth year not played: the town's work at once
./release/Chronicle --save save/other.sz      # another save file
./release/Chronicle --month-ms 1000           # a month every second instead of every 2 (a child's months are shorter in proportion)
./release/Chronicle --walk hollow             # a life never written, opened on a walk (hedgerows, millpond, hollow, quarry)
./release/Chronicle --walk hollow --walk-seed 7   # the same walk with the same dice every time
./release/Chronicle --build keep --walk hollow    # a man of the Keep on the Hollow's site, every place known 4 of 4: its beasts and its boss to fight
./release/Chronicle --build hand --set dex=25 --set flask=3   # a build on the Life page, figures set on top (--set know.<key>=<n> for what the world knows)
./release/Chronicle --build list                  # the builds (rules/testbuilds.pg, the fight matrix's too): plain, blade, scholar, street, hedge, keep, collegium, collegium.dropout, spellblade, hand, greenwood
./release/Chronicle --dev TypeSpecimen --theme candle
```

**His sixth year.** Every life opens at 6 years old with 4 of each part, 35 of Vitality and
nothing held, and its first year is made of short works, a month or two each, that shape the
boy he is at seven: *Glean Behind the Reapers* (his first rations), *Carry Flour to the Baker*
(his first coin), **one of two** (*Climb the Mill Wheel* for his body, Strength and Dexterity,
or *Sit with the Old Scribe* for his mind, Intelligence and 5 of Vitality: the one not taken
has no tile any more), *Carry Books for Father Anselm* (a keepsake tome, kept in the Inventory;
nothing asks for it yet), then **two of three** works of two months that open at 6 years 4
months and are over by 6 years 9 months (*Learn Knots from the Bargemen*, *Haul Sacks at the
Mill*, which takes 5 of his Vitality, *Sing at the Chapel Door*: there is time for two, each
says how many months are left to begin it, and the third closes), *Go to the Market* at 6 years 8 months
(the Town page, where his coin buys the rations he is short of by then), *Bring in the
Harvest* (two months, a point of each part) and *Explore the Outskirts* at 6 years 11 months,
a story of four days (his lunch, one of three ways out of the town, its fork, and a slime in
the ditch: the stories, below) which ends the day he is seven. From there the town's work is
his, the whole list at once.

**A first life** (a browser with no save, or `--fresh`) is shown the year one work at a time,
and a **guide** walks him through it: one short sentence
at a time on a leaf under the one thing it is about (a tile, what he holds, a stat, the clock),
a pointing hand beside that thing and a shade over the rest of the page, which can still be
pressed. For a choice every tile of it is left lit. The leaf has two buttons: *Skip tutorial* ends it for good, and *Next* passes a step
that would otherwise wait a few seconds (a step that waits for a press has none). A work is begun in two presses, and the hand shows both: first the task,
then, once it is chosen, the Begin button, while the leaf says that a
double click on the task begins it too. The steps: begin a task, his food, the flour, his
coin, the first choice, his stats, the tome, two of the three, the market (then the town's own
steps: its tab, buying rations, which the step waits for, its places), the harvest (with the
hand on the Life tab while the Town page, or on a phone another tab, is the one in view), his age
and what the game is about (keep him alive to 30 at least), then the outskirts' row in the list
and, in the outskirts, his lunch, the three paths, the card at the ditch and, in the fight, the
slime, the order of the turn and Strike
(each said on the screen it points at, over the shade of the walk's screen), and the classes the
day he is seven. After its last sentence nothing of the guide is left. A
few lines of the town's story are written in the log on the way. The second life and every later
one play the same year with no guide and its works all listed from the first day, in whatever
order the player likes.

**The town.** *Go to the Market* (a month of his sixth year, in every life; its tile stands on a gold
ground, and no age closes it, so that no life misses it) opens the **Town page**: it takes the place of the list of
activities in the middle column, by the `Town` tab, and the two other columns stay. The tab is
not on the page at all before that. When it comes, the guide shows it, then the market where his
coin buys rations, then the places, once in a world, in the life that first goes there. Whatever is
new on the page that is not in view (a task, a thing sold, a place) is counted in a small round
at its tab's corner until the page is opened. The Adventure tab does the same for the places to walk to: it comes with a count of the places behind it the day of his first walk, and counts a place whose tile comes onto its page, open to him or not, and a place that opens (`newInAdventure`, `stoodWalks`, `openedWalks`). It shows the places of Bellmoor, each with three levels, and under them
the market, where what is bought on the spot is bought (reached by the Town tab: no tile of the Life page goes there), on two lines, *Buy* and *Sell* (by the rules' `trade`), what is made there under *Make*. A place is raised by a work that stands under the places, on the same page (a click on a
place chooses its work, and Begin begins it): months, a little coin, and mainly resources: the
first level of a place two basics, the second more of them and gems, the third more again,
gems and a thing found on a walk; for some a stat or years of his life as well. What is raised
is the world's and outlives him: the next
life finds that town the month it goes to the market (its page is not open before: the places stay raised, the page is each life's to open), with what its places give (a shorter task, a class that asks one
less, a skill point at birth, a familiar slot, a kind of familiar to bind, more room in the stash,
more from each familiar of a kind), never a class for free. At his death the coin he holds, and
what he holds of the things a place's next level still asks, can be left to one place, and count
toward that level; what a raise does not use stays for the level after. The page grows with what he does
(`shows` in `towntable.pg`): two places the day the town is known (the Market, the Mill), the
Smithy once he has worked a term at the forge, the Yard to a veteran of the Border Campaign and
the Watch Gate to a captain, the Chapel the day he enters the Collegium, the Collegium Gate after
Magister Orin and the Harrow Road after the expedition to Harrow, the Inn from the second
life of a world, the Storehouse once the stash holds anything (what is drawn from the stash is
drawn there, and its levels make the stash keep more) and the Hunters' Lodge once he has walked
the Hollow Woods. The smithy forges gear once it has its second bellows (an iron
blade, a leather jerkin, and a warded charm at its second level once a marsh light's working has
reached him): a piece once a life, held under GEAR, taken into every fight and lost at his death.
The day a place comes its first work comes with it: that work is how it is
built. A place that comes later is
said in the log and by a toast, and a place a life has seen stays on the page of every later one.

**The walks.** After a few terms of work a tile opens the first walk outside the town, *The
Hedgerows*: a tile of the **Adventure** group like any other, with its months up front. Begun, it
does not run its months: it opens the **on-site screen** over the page, where the stay is a count
of days (action points, `AP`) spent one choice at a time: seek a beast or watch it from cover,
gather, study the place, rest, turn for home. Each choice says what it costs before it is made,
and each one made is a line of **the record**. A beast found is fought on the **fight screen**,
turn by turn. Every beast he fights has a panel of its own across the top, the size of his, over
him and over his turn, which stand under them side by side: its name, how it stands, and the move
it means to make this turn, by its name alone. What the move is, is in its gloss, and what it
does to him is said there only once it has been used on him or once he has watched the beast.
A blow is seen coming: its rule fills, his or the beast's, then the blow is told in the account,
the rule of who was hurt flashes and falls, and what was taken lifts off it (a blow that found
nothing lifts a small *MISS*). The timeline has a panel of its own at the top of the right
column, over the account: his actions over a line of time and the beasts' under it, where each
falls, with the end of his day across it; the move under the mouse shows where it would put him
(his old place faint, a beast it holds put off, a beast that now acts before him in red), and
under it what is coming at him. What he may do is a
button each, pressed and done, its gloss saying what it would do: what anyone can (strike,
defend, take a remedy while he holds one, flee) and what his life has taught him and nothing
else: the Keep's blows to its sworn men, the Collegium's workings to its scholars, a trick of
the streets to who knows them. Every few turns of a fight a day of the stay is gone. When the days are spent, or he turns for
home, the walk's months run as any work's do, and the record is kept: the **Adventure page** (a
tab beside Life and Town, there once he has been on a walk) lists every place he knows of, the
open ones first, and from then on the Life page keeps none. A line says what kind of walk it is,
its months and days, what it gives and how well he knows it; a closed one, what it asks and what
he is short of. The place chosen has a button that sets out for it anew, by hand; under it a
table of the walks he has kept of it, its last five, the best first; and under that the form of
the one chosen: walked again so many times, with what that takes, uses, brings and teaches, and
no day chosen and no fight fought. A place that opens to him while the page is not in view is
counted at its tab's corner. What a walk brings is his, in his Inventory:
some of it is sold at the market (berries, pelts...), some is what a place of the town is built
of (firewood, clay, ash wood, stone). It ends with him: at his death, with his coin, only what
the place he chooses still asks of it is left to that place. What walks teach (a zone studied, a
beast watched or fought) is the world's, kept from life to life; the records are the one who
walked them's. The on-site screen is laid as the Life page is: how he stands (his Vitality, how
well the place is known), what he carries and the way home at the left, what he may do in the
middle as tiles, what he has taken and the record at the right.
The last place to walk to is Millbrook Marsh, from 18 once the Quarry Road is known 4 of 4,
three months a walk: its lights are workings that a ward turns and armour does not, its leech
mass mends unless it burns, and the Lantern Mother over its deepest pool calls her lights as she
is hurt and, beaten, teaches the marsh wisp. It gives what only it has (pitch, bog iron from 3
of 4, and now and then a light's bead of wisp-glass), held among his stores, never sold, and asked
by the town's last levels.

**The fight** (0.7). Who is quicker acts more often: the timeline shows who acts when, up to the
end of his day, and a quick move holds him less than a heavy one. A blow has a type (cut,
pierce, blunt) and a working an element (fire, frost, storm) or holiness; a beast resists some
and is weak to others, has armour against blows and a ward against workings, and a big one has
poise that his blows wear down until it staggers. Each class plays a rhythm: the Keep builds
momentum blow by blow and spends it on a finisher, the Collegium sets one element on a beast and
another off it, the Hidden Hand strikes many times before the beast's turn, the Greenwood sets
traps on the walk and shoots first. A boy's tasks teach a first tool of each kind, and every
tool he has learned is his in every fight. Who can hide, has set a trap or holds a ration
chooses how to meet what he finds. Each place has its boss, met by choice, beaten once a life:
the Old Pike, the Old Shell, the Old Grey, the Lantern Mother. Lose to a boss and the life ends there; be brought
down by any other beast and he comes home without what the walk had gathered. A kill trains
nothing in him: the first of a kind in a life is a lesson about it, and a technique used often
rises a rank.

**Written time.** The time the chronicle stays shut is kept: a second for a second from the
hour the save was last written (`LifeSave::savedAt`), an hour of it at most, and none for less
than a minute away. So is the time the months stand still in the game with nobody at it
(`LifeScene::runStill`): a minute of them with no key, click, wheel or mouse movement, and that
minute and every second after it are written, until an input or months that run. It is a holding of the rules (`resources.pg`, the one with a `haste`; its
limit is `lib.pg`'s `statLimits`), so it stands in the Inventory with his coin and his rations,
in seconds and from the first ("0/3600": `always` on its row), but it is the player's: a new
life has what the last one held. The button *Months ×1* (or `F`) spends it: a press goes ×1, ×2, ×3 and back, and the
months run that many times as fast. The speed multiplies the pace of his age
(`LifeScene::monthMs` is not changed by it: the month under way runs faster through the same
length), a month run so takes the row's `aMonth` of it, 7 seconds at ×2 as at ×3 (a week of the
player's for a month of his), and the frame it runs out the months
are back at their own pace. Nothing of it is spent while the months are stopped. The button is
shown while any is held (a first life's, once its guide is over), at the right of *At work
now* on the line of its last button (*Pass a month*, *Begin*, *Go on*): it is a page node
anchored to the panel's bottom right, and under a running row the panel keeps it a line
(`hasteRow`).

With no `--dev` the game opens on the **Life scene** (`--dev LifeScene` names it too).
`--dev <Scene>` picks a dev gallery instead; an unknown name exits with code 2 before a
window opens. On the Life scene, confirming an activity starts the months (one every 2 s,
`--month-ms`), and they stop when its term ends, waiting for the next choice. A child's months
are a part of that (`LifeScene::monthMs`): half of it in his sixth year, three quarters until
he is ten, so a new player is not kept waiting. The running
row's rule fills through each month in the month's own time. `SPACE` pauses or resumes work
under way (a paused month keeps what it ran), and the head of *At work now* says which:
`RUNNING`, `PAUSED · SPACE` in red, or `IDLE` between two works. A work that is stopped (by
the key, or by a part that fails) has a *Go on* button under its row (`LifeScene::resumes`): a
phone has no key. At nothing no key runs the
months: *Pass a month*, the button standing in *At work now* in the running row's place, passes
one. `M` passes one month at any time (a dev key), `T` switches the theme, `R` reduced motion, `S` saves, `F` spends written time (as its button does), `N` starts
a new life. The life is also saved on its own, with no line in the log: after every month, every choice, every thing done
at once and every new life (never with `--no-save`); `S` writes it and says so. A life whose Vitality reaches 0 is lost: its **ending** comes up over the page, a leaf
saying who he was, how it ended and what the chronicle keeps of him, and nothing passes until
*Begin a new life* (or `N`). The next one begins at 7 with 12 rations, the
months stopped and a line in its log saying how the last one ended. His prime ends at 30: every
activity of it closes, only an old man's work and the market are left, and from there the most
his Vitality can be falls by five every two months, until that life ends too. Vitality stands under the
clock's age at the top of the right column, his other parts in a panel under the log. A month
that takes from it turns it red for a moment, and the first such month stops the running
months, with a line in the log naming the part and the key that goes on. What was taken mends, five every two months, up to the most it can be: the hatch on its
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
- **Rules** ship compiled when the tree knows the native `ChronicleRules` tool: built in a native
  tree (`make ChronicleRules`) and given once with
  `cmake -DCHRONICLE_RULES_COMPILER=<native tree>/ChronicleRules .`. The tool compiles them as
  the game does (`precompileRules` in `Core/rulevm.cpp`) into `chronicle-rules/` of the web tree,
  again whenever a script or the tool changes, and the pack holds those `.pgc` alone at
  `examples/Chronicle/rules`: `Rules::load` takes a `.pgc` where its `.pg` is missing, and the
  browser compiles none at start. The tool has to be built from the same engine as the web
  build (a bytecode of another revision may not mean the same). Without the tool, the rules are
  packed as `.pg` and compile in the browser at every start. No other `.pgc` is packed from the
  source tree, where one may be stale.
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
  `{"age":12.50,"world":66,"aim":"warrior","running":"yard","monthsIn":2,"terms":14,"deeds":2,"log":31,"lives":3,"picks":22,"begun":15,"atOnce":4,"skips":2,"guide":10}`.
  `lives` counts the lives of this browser, this one included (1 for a save older than the count).
  `picks`, `begun`, `atOnce` and `skips` are what the player pressed in this life: the tiles he
  chose in the list, the works begun, the things done on the spot, the months passed by the
  button with no work at hand. `guide` is the last step of the first life's guide that ended
  (the `order` of `rules/achievements.pg`, 0 before the first, 13 once it has said everything,
  10 before 0.6, whose guide had ten steps and other works; a
  later life keeps 0, and a digest from before 0.3 has no such key). `v` is the build the digest
  is of (`LifeSave::DigestBuild`, "0.6" for the first that says so): changed with what a digest
  means, so that two builds' rows are told apart. `townKnown` is 1 once
  this life has been to the market (before 0.6: once a life of this browser had explored the town), and `town` the levels of its places added up (0 to
  30). `walks` and `replays` are the walks outside the town he made by hand and the ones a record
  made for him in this life, `records` the records he keeps, `onWalk` 1 while he is on site (a
  digest from before 0.5 has none of the four). The same place by place: `walk.<zone>` and
  `replay.<zone>` (`"walk.hedgerows":2,"replay.hedgerows":5`), a key only for a place he went
  to. `carried` counts the walks he was carried home from and `queued` is the longest queue he
  set going. `fights` are the fights he came to on walks made by hand (a record plays none),
  `fightsWon`, `fightsLost` and `fightsFled` how they ended, and `fightsAuto` the ones that
  ended in the hands of his stance. `known` is what the world knows of the lands outside,
  added up: the places studied and the beasts watched, life after life. The works of the end
  of a life he has done are named one by one with how often, `did.<id>` (`"did.serve":2,
  "did.campaign":1`): the ways into a class, and every work that opens at 16 or later, a
  class's own and old age's. So are the works of his sixth year, what opens before 7
  (`"did.scribe":1,"did.knots":1,"did.sing":1` is the mind, and which two of the three):
  `LifeScene::namedWorks`, from the rules' `enters` and `fromAge`.
  A work he has not done has no key. `guideSkipped` is the step
  that was being said when the player pressed *Skip tutorial* (0 when he did not): `guide` then
  reads 10. `deeds` counts the deeds
  alone, not the lore read nor the guide's words.
  The scene gives it after every month, choice, thing done at once, step of the guide and new life.
- **What it is shown on** rides at the end of the digest, the first row's included (which has
  nothing else: the scene has not spoken yet). The scene adds `"page"`: `full`, `compact` or
  `phone`, the page the life is read on. The browser is asked the rest as the row is sent:
  `"viewW"` and `"viewH"` (the view the game is drawn in, CSS pixels), `"screenW"` and
  `"screenH"`, `"dpr"` (the pixel ratio) and `"touch"` (1 when the screen takes a finger).
  Sizes, to know which pages to draw for: nothing of who plays. A row from before 0.4.1 has none.

```sql
-- How long a session lasts, and how old he was when they stopped
SELECT session_id, MAX(session_duration_ms) / 60000.0 AS minutes,
       (ARRAY_AGG(save_snapshot ORDER BY timestamp_ms DESC))[1] AS stopped_at
FROM analytics_events WHERE event_type = 'chronicle.session_end'
GROUP BY session_id ORDER BY MIN(timestamp_ms) DESC;
```

`tools/chronicle_stats.pg` makes the whole report from a CSV export of the `chronicle.%` rows
(the query is in its header): sessions and their length, the days, how far a session got, how
many first lives got past each step of the guide (a drop between two steps is a drop between two
sentences), one funnel a build since the steps changed with 0.6; his sixth year, by the works
the first lives did, which of the two and which two of the three; where a session stopped, the lives lost, and the screens: how many came with a
finger and how long they stayed beside those with a mouse, the views' widths against the game's
own steps, the page read, and the views most seen.

```bash
release/PgCompilerBootstrap tools/chronicle_stats.pg chronicle-events.csv report.txt
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

- **Mark** (`UI/mark.h`) — one of 29 glyphs at one of five kit sizes
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
  the time and, for a work that feeds him, the rations it gives up front (`Rations  3 up front`),
  then a ruled section for each of REQUIREMENTS, COST and REWARD,
  each row under the stat's full name (`Strength  +1`), a requirement green when he has it and
  red when he is short. Its only footnote is `AT WORK NOW`, on the work he is at: what a locked
  one still asks is in red under REQUIREMENTS, and when it closes is the tile's to say. The tooltip form is registered under a **key** on
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
  (work that takes months, the leaf's own folio), `activity.kind.instant` (a thing done at once, a
  transaction: ochre) and `activity.kind.locked` (what he cannot do yet: vellum-tint), with
  `activity.kind.running` for the work he is at, and `activity.kind.selected` for the tile chosen
  (vellum-tint, with the focus-ink edge). Hover deepens the kind's own ground (`.hover`).
  The setters still keep what a compact row does not show (`setCount`, `setPercent`,
  `setRequirement`), for whoever says it. A **tile** (`tile`, a compact row stacked for a grid)
  is framed (`activity.tile.frame`; `.urgent`, the loss's colour, while its closing is near and
  it is not locked). Its mark (S16, `activity.mark`, or its kind's colour) stands before its
  name, set in `control` (`activity.tile.name`), wrapped in the width left over two lines at
  most, then elided. Under them its last line: at the left an hourglass and its time
  (`activity.tile.cost`, in `tick`; nothing for a thing done at once), at the right the months
  left to begin it after a stopwatch (`status` and `statusMark`, `activity.tile.status` and
  `activity.tile.chrono`, in `tick`; `setCloses`: "14 mo", "last mo"; `.loss` when `urgent`),
  and nothing for what does not close; `.locked` while he cannot take it. What a locked tile lacks is its gloss's to say. The work he is at says nothing there. The
  single frames are 2 in the theme: the frame's shader draws a 1 px rule faint, and on a tile
  whose width is not a whole number of pixels its right edge would not show. A `major` tile is framed in gold and an
  `adventure` one, which takes him out of the town, in red, each on a ground of its own. `natural` is the height its content
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

- **PlaceTile** and **PlaceGrid** (`UI/placetile.h`) - a place of the town, as large and as
  plain as an activity's tile: its mark and its name, a seal a level (`place.seal.earned` in
  gold-edge, `place.seal.empty` in rule-hair) and a line of caps for what it gives now. Its gloss
  says the rest. A click sends `PlaceSelectedEvent`; the tile does nothing with it, whoever holds
  it lights the one chosen (`PlaceGrid::select`, `place.ground.selected`). The grid lays its
  tiles as an `ActivityList` lays its own: as many to a line as fit at `tileWidth` or more, 8
  apart, sharing the width. Kinds `PlaceTile` and `PlaceGrid` (records `places`); gallery
  `--dev TownGallery` (`res/chronicle/ui/towngallery.yaml`).
- **Tabs, a page not open yet**: `Tabs::setEnabled(ecs, index, false)` leaves a tab faint
  (`tabs.tab.off`), out of the Tab order and deaf to clicks; `Tabs::setShown(ecs, index, false)`
  draws nothing of it at all, and it keeps its room (for the last of a row: the town's tab, until
  a life has explored the town). `Tabs::setNotice(ecs, index, count)` puts a small round with a
  count (`tabs.notice.ground`, "9+" past nine) at the tab's top right corner, over the gap to the
  next tab, so no face moves.
- **A featured tile**: `ActivityRowSpec::featured` gives a compact row the ground
  `activity.kind.featured` in the place of its kind's, while it can be chosen. The rules set it
  with `featured: true` on an activity (Go to the Market).
- **A major tile**: `ActivityRowSpec::major` frames a tile with two gold rules
  (`activity.major.frame`, `.locked`), shows its mark in gold before its name
  (`activity.major.mark`, `.locked`) and gives it the ground
  `activity.kind.major`. It holds in every state. The rules set it with `major: true` on what
  turns a life: the four ways into a class and Go to the Market. Such an activity also carries
  `about`, a sentence on what it does and a word of the town's, which its gloss says under its
  name in the place of its group.
- **What is not counted in months.** `ActivityRowSpec::cost` (a row, a tile) and
  `Button::setCost` (a button made with a cost: `cost: "2 AP"` in a file) write a text in the
  place of the months, for what a walk counts in days. `EventLogSpec::steps` (`steps: true`) makes
  a log's first column a count the caller gives in `age` (a step of a record, a turn of a fight),
  written whole, a middle dot for a negative one; such a log asks no order of its lines.
- **A list filled twice in one pass** (`ActivityList::setRows`) lets go of its first fill too:
  the layout takes what it is given a pass later, and the list keeps what it handed over
  (`ActivityListState::given`) until then.
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
compile here (a script importing a compiled module fails in the compiler's
`PoppingJumpPass`), and it would not follow an edit of `x.pg` either. So `milestones.pg`, which
the game loads, only imports `milestonetable.pg`, which holds everything and is what
`forecast.pg` imports; `activitytable.pg` is imported by `windows.pg`, `forecast.pg` and
`epitaph.pg` and never loaded. `lib.pg` is never loaded.

**One run, kept.** A run executes its script's whole top level, tables and all, so `Rules`
(`Core/rulevm.h`) asks as little as it can. `windows.pg` answers both `Rules::activities` and
`Rules::windows` in one run (it shapes `activities` through its import), and every answer is kept
while what it was asked with stands: the age, the character, `done`, `world` and the call's own
arguments, compared by value. `Rules::nbRuns` counts the runs.

| script | inputs | outputs |
|---|---|---|
| `lib.pg` | - | pure helpers: `clamp`, `ordinal` ("14th"), `monthOf` (whole months lived), `monthsBetween`, `monthsToAttempts`, `signed` ("+1" / "−2"), `fmtMonths` ("6 mo"), `numberWord`, `statName`, `statOf`, `lookup`, `raised` (a gain stopped at the stat's ceiling), `byId`, `pathFlagOf`, `asksOf`, `openNote`, `shiftIn` (a month's change to a stat); `lastAge` (30, where his prime ends), `anyAge` (the `finishBy` of what no age closes) |
| `economytable.pg` (imported by `activitytable.pg`, `resources.pg`, `town.pg`, `depgraph.pg` and the solver's `life.pg`; never loaded) | - | `const` tables: `basicResources`, `madeResources` (`{id, name, one, glyph, about}`, a basic with `lot`, `coin`, `price` for the market); `scavengeRows`, `storeRows` (activity rows); `familiarKinds` (`{id, name, one, glyph, known, about, train, eats[], makes[], bind[]}`); `slotGivers` (`{stat, slots, needed}`); `gearPieces` (`{id, name, glyph, might, armour, ward, about, forge {costs[], months, smithy, onlyWith?}}`); `economyKnobs` (`stashMost`, `baseSlots`, `rationsAMonth`) |
| `activitytable.pg` (through `windows.pg`) | `age`, `character`, `done`, `activityId` (`""`: all; `forecast.pg` shapes only its own), `world`, `lives` (which life of the world this is: the places of the town read it), `running` | `activities`: `{id, group, name, glyph, months, rank, each, path, enters, board, upFront, upFrontLasts (the months its rations last with his familiars eating them, when fewer than its own; 0 otherwise), fromAge, finishBy, gains[{stat, amount, label, name, hidden}], costs[{stat, amount, label, name}], requires[{stat, label, current, needed}], needs (the requirements as written, for `depgraph.pg`), locked, done, uses, left, spent, closed, pathOpen, listed, showAge, showFrom, place, trade ("" or "buy", "sell", "make": a row of the market), tally ("DONE 2", "DONE 0 · 1 LEFT"), until ("" or "CLOSES IN 14 MO"), urgent, after[step]}` |
| `milestones.pg` (`milestonetable.pg`) | `age`, `world`, `lives` | `milestones`: `{age, id, label, passed, entry, asks[{path, stat, label, needed}]}`; `next`: `{id, label, age, in}` (`in` = months to it; `"", "", -1, -1` past the last) |
| `windows.pg` | `age`, `character`, `done`, `activityId` (`""`) | `windows`: `{id, name, from, to, state (upcoming / open / closed), note (in attempts), attempts}`, one per door of an activity whose path is open to him |
| `forecast.pg` | `age`, `character`, `done`, `activityId`, `monthsIn` | `forecast`: `{atStart{stats}, atTerm{stats}, percent, toward (the percent a month on), months, caption ("MONTH 3 OF 6 · STRENGTH 14 → 16 AT TERM"), gaps[{stat, label, current, needed}], entries[{text, kind, figure, glyph}], upFront, unworked, takenBack, begins[{text, kind, figure, glyph}], leaves[{text, kind, figure, glyph}], atLeave{stats}, atOwed{stats}, error}` (what a work that feeds him gives as he begins it, and gives back when left before its term) |
| `resources.pg` | `age`, `character` | `month`: `{after{stats}, entries[{text, kind, figure, glyph}], hurt[stat]}`; `rows`: `{id, group, groupLabel, glyph, name, tone, rate ("+2 / mo"), limit}`; `glosses`: `{id, title, text, footnote, rows[{label, value, tone}]}`; `caps`: `{stat, most}`; `death`: `""` or the line the next life opens with; `warning`, `advice`: what he lives on running out, counted with his familiars' share; `eats`: the rations the coming month eats, his own and his familiars', fed or not (the solver reads it) |
| `achievements.pg` | - | `achievements`: `{id, kind (deed / guide / lore), name, entry, say, point, hold, order, asks[{fact, op, value}], gives[{stat, amount}], until[{fact, op, value}]}` |
| `epitaph.pg` | `age`, `character`, `done`, `activityId` (`""`), `deeds` (the names of the deeds reached) | `epitaph`: `{cause ("He died an old man, in his thirty-third year."), story[text] (what he became, worked at, left, and what is told of him), text (the story as one paragraph), tally ("AGE 33 · WORKS 14 · COIN 31 · DEEDS 2"), ask ("" or, past `lastAge`, the line that asks for a comment or a rating)}` |
| `town.pg` (`towntable.pg`) | `age`, `character` (with `town_known`, `town.<place>`, `fund.<place>`, `fund.<place>.<thing>`, `seen.<place>`, `stash.<id>`), `done`, `world`, `lives` | `places`: `{id, name, glyph, about, level, most, shown, gives, line, tale, built, fund, nextWork, nextName, nextMonths, nextCoin, nextYears, nextCosts, nextGives, nextLocked, gaps[{stat, label, current, needed}], given[{level, built, gives}], tiers[{level, built, gives, reached}], leaves[{stat, amount}], leaveLine, spends[{stat, amount}] (what the fund pays of the next level, `coin` its coin, taken from the fund when it is reached), stashLine}`; `start`: `[{stat, amount}]`; `opened`, `raisedLine`, `epitaphLine`, `fundLine`, `foundLine`; `gift`: `{stat, before, after, plain, chosen, things}`; `stash`: `[{stat, most}]` (what the stash keeps at his death, and the most of each); `stashLine`: `""` or the ending's sentence of what goes into the stash with no gift (a place's `stashLine` says it with the gift left to it) |
| `expedition.pg` (`expeditiontable.pg`, `expeditionlib.pg`, `fightlib.pg`, `storylib.pg`, `storytable.pg`) | `age`, `character` (with `know.<key>`), `who`, `seed`, `ask` (enter, act, look, records, begin, term, builds, plan; enter with `act.kind` "story" begins a story), the walk as it stands (`run`, `record`, `marks`, `fight`, `foes`, `account`), `act` `{kind, target, count}`, the records (`recipes`, `recipeSteps`, `recipeMarks`), `walks`, `queue`, `replay` | the walk as the run leaves it; `after{stats}`, `entries`; on site `site`, `seeks`, `works`, `card`, `options`, `bag`, `taken`, `home`; a fight `duel`, `menu`, `targets`, `order`, `hero`, `foe`, `preview`; the records `shelf`, `form`, `stops`; `recipe`, `blocked`, `stopped` |

**Repetition** (`activitytable.pg`). `done` is the save's count of terms completed per activity.
An activity may carry two optional fields:

- `uses: N`: it can be done N times. `left` counts down and `spent` turns true at the last
  one; the Life screen drops a spent activity from its list.
- `after: [{uses: N, ...}]`: what repetition changes. `rank`, `months` and `gains` in a step
  replace the activity's own once N terms are done (an upgrade). `effects [{stat, amount}]`
  and `entry` happen once, at the term that makes it N (a trigger): `forecast.pg` adds the
  effects to `atTerm` and the entry to `entries`.

**Costs and things done at once** (`activitytable.pg`). `costs: [{stat, amount}]` is what an
activity takes: it is listed in `requires` (he must hold that much, or the row is locked) and is
taken **the moment it begins**: `forecast.atStart` is the character once it is paid, and the
scene takes it on confirm; `atTerm` does not take it again. The row shows each cost as a
negative figure beside the gains. `months: 0` is an activity done at once: confirming it applies
`atTerm` (its gains on what is left once it is paid) immediately, no month passes, and it can be
done while another activity runs. Its row reads `NOW` where the others read their months.

**Ages and paths** (`activitytable.pg`). `fromAge` (default 7) is the youngest he may begin it:
younger, the first thing it asks is the age. `finishBy` (default `lastAge`) is the latest its
term may end, inclusive: an 18-month activity with `finishBy: 20` is begun by 18.5. Past that
it is `closed` (`anyAge` for what no age closes). A closing is said ahead: `until` is the
world's date of the last month he may begin it, month/year (`"CLOSES 5/11"`, or `"CLOSES THIS
MONTH"`), said from `closingShown` (24) months before it, `urgent` from `closingUrgent` (6).
The date does not move as the months pass; the scene writes it on the row
(`activity.<id>.until`, `.urgent`) every month, so no row leaves the list unannounced, his
prime's at 30 included. `path` puts an activity on one of `paths` (`warrior`, `mage`, `thief`, `renegade`, each with
the flag that makes him one of it); the one that `enters` it is open until he is one of any
path, the others only once he is one of theirs (`pathOpen`). The paths exclude each other. The
page calls a path his class ("A class", "Choose a class"); the rules keep the word `path`. The
renegade is the class of who took none of the three in time, and of no one else: its way in
("Take to the Greenwood", `lastResort: true`) has no row while another way into a class can still
be begun, nor while he is at one (the rules are handed the work at hand, `running`), nor once he
is of a class. It comes into the list the month the last of the three closes (19 years and 7
months), with a line in the log (`opens`), asks nothing and stays open until 29. A row
is `listed` while its path is open, it is neither spent nor closed, he is of age for it, and
he has most of what it asks: `reach` is the average share (each up to 100) of its requirements
he meets, costs and room left out, and the row shows from `reachShown` (75). An activity may set
its own two: `showFrom` (the share that lists it, 0 for whatever he has) and `showAge` (the age
its row is listed from, before he may begin it: it stands locked, the age the first thing it
asks). The great step of each age carries both, so it shows ahead as what the years he is in
lead to: the three ways into a class from his first day at 7, a path's proving from 16, its
mastery from 21 (the renegade's from 20 and 23). Apart from those, a boy is not shown
a life of locked rows; the doors on the clock still say what is coming. The scene shows the
listed rows only, and rebuilds the list whenever what is listed, or what a row asks, changes. A requirement
is `{stat, needed}`, and `needed` is 1 when it is left out (`{stat: "letters"}`: he has any). It
may be `eased: {flag, needed}` (with the flag it asks less) or `anyOf: [flags]` (any one will
do); `bonus: {stat, needed, gains}` replaces the gains when the stat is high enough (`needed`
1 when left out, the same). Every `{stat, amount}` of the activity and town tables (gains, costs,
`more`, `effects`, a level's `costs` and `start`) may leave `amount` out too: it is 1, filled in
by `lib.pg`'s `amountsOf` before anything reads it. Every age
test counts whole months (`monthOf`), so an age summed a twelfth at a time meets its doors on
time.

**Rations up front** (`activitytable.pg`, `forecast.pg`, `resources.pg`). He eats a ration every
month (`economyKnobs.rationsAMonth`), and his familiars eat theirs, whatever he is at: there is one
way the rations go. `board: true` is work that feeds him, and its row's `upFront` is the rations
of its months, given as he begins it (forecast.pg's `atStart`, with the costs; its `begins` is the
log line, *Carry for the Carters gives its rations up front*, `+3 rations`). They are kept past
the limit of sixty, as any gain is; a work done at once gives none. Its tile says `RATIONS UP
FRONT` and its gloss `Rations  3 up front`. Left before its term (a work set aside for an
answer that closed before he could go back to it), the rations of the months not worked go back,
never more than he holds (forecast.pg's `takenBack`, `atLeave` and `leaves`, at `monthsIn`). A
work set aside and taken up again keeps them: the months it gave them for are still ahead. A boy
who never held a ration eats none, and the gleaning of his sixth year, whose term is his first
rations, does not feed him. A save from before (`LifeSave::boardUpFront` false) in the middle of
a work that feeds him is given the rations of its months left once, as it opens (`unworked`,
`atOwed`). Eating the last ration is still a fed month: only a month begun with none costs
Vitality.

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
The basics of `economytable.pg` (`basicResources`: berries, water, firewood, stone, clay) and what
is made of them (`madeResources`: gems) are holdings of the group RESOURCES, each with its limit;
every kind of familiar is one of the group FAMILIARS (`fam.<kind>`, how many he has bound), and
its gloss says what it eats and makes. A new group needs no C++: a holding with a new
`group`/`groupLabel` is enough.

**Limits** (`lib.pg`). `statLimits` is the most he can hold of a stat (`limitOf`, 0 for none;
`roomFor` is what he can still take in). What a month produces stops at the limit
(`resources.pg`). An activity that brings such a stat asks for room (`activitytable.pg`: one more
requirement, `Room for Rations`, current = the room left, needed 1), so it is locked while he
is full. Begun with any room left it brings all it brings: **an activity may carry him past the
limit**, and what he holds past it is only eaten into, never cut.
The basics have limits (berries 30, water 40, firewood 30, stone 20, clay 20, gems 20), and a
store raises one for the life: `limitOf` adds the stat `store.<id>` to the base, and the rows of
*Your stores* (`economytable.pg`'s `storeRows`, listed once he holds some of the resource,
`onlyWith`) bring 20 of it a term, three terms each. What familiars make stops at the limit
(`shiftIn`); scavenging (`scavengeRows`, the group *Scavenge*, each term also counting the stat
`scavenged`) is locked by its room like any other row. `scavenged` is one of `lib.pg`'s
`hiddenStats`: a count the rules keep for themselves, gained like any other but marked `hidden` on
the row's gains, so no tile chip, gloss REWARD line or log figure shows it (what asks for it still
names it). *Dry berries* (`dry.berries`, three berries to a ration, at once) stands in *Scavenge*
too, with no place, from his first berries (`onlyWith`; what it asks is only its cost): a sprite's berries feed him before he knows the town. **A walk ignores limits**: its haul is
never thrown away. A save from before the limits keeps what it holds past them.

**Caps and mending** (`lib.pg`, `resources.pg`). `statCaps` names the stat holding the most
another can be (`vit` -> `vitmax`). What takes from the stat leaves the cap alone; a gain from
an activity raises both (`forecast.pg`). `mending [{stat, amount, every}]` brings the stat back
toward its cap, `amount` every `every` months counted in `<stat>rest`; a month that took from it
mends nothing and starts the count again. `caps` (`[{stat, most}]`) is what the scene shows as
the part's ghost when no activity is chosen, and as `At most` in its gloss. A save from before
the cap takes what it has as its most.

**Familiars** (`economytable.pg`, `lib.pg`, `resources.pg`, `activitytable.pg`). A familiar is a
producer bound to him for life: the stat `fam.<kind>` counts how many of a kind he has bound, so
it is held, shown in the ledger and lost at his death like any other stat. `familiarKinds` gives
each kind its `eats`, `makes` and `bind` (what binding it costs), its `train` lesson's name and
whether it is `known` from the first. Every month `resources.pg` feeds them after he eats
(`feedFamiliars`): kind by kind in the table's order, **all or nothing per kind**. A fed kind
takes what it eats and makes what it makes, stopped at the limit; a hungry kind makes nothing and
costs nothing else, and its hunger never takes his Vitality. A work that feeds him gives his
own rations, never his familiars'. `eats` (an output) is the rations the coming month eats, his
own and his familiars', which the rations warning, the *lasts* line and the solver read. Both it
and the gloss's *Lasts* count the months
that feed him and his familiars both (`monthsFed`: nine rations at two a month last four), and the
gloss names the familiars' share (*His familiars eat*).
- **Binding** (`bind.<kind>`, the group *Familiars*): a month, the kind's `bind` as its costs, and
  a free slot (the requirement `slot: true`, whose current is `freeSlots`): while none is free no
  bind row is listed (the lessons are), and one that comes back with a slot is no "New:" toast
  when it has stood on the page before (`listedLast`). The first known kind also waits for
  two scavenges (`scavenged` 2); a kind not learned is not listed (`onlyWith: know.fam.<kind>`).
  Its tile says the upkeep ("EATS 1 RATION · MAKES 4 BERRIES") and its gloss ends on it in a
  sentence ("It eats 1 ration a month, and makes 4 berries.", `upkeepSentence`), so it is known
  before he binds. A lesson's gloss says what it adds (`lessonSentence`).
  Nothing unbinds: when slots fall below the familiars bound (a relic spent), only the binding
  rows lock.
- **Slots** (`slotsOf`): `economyKnobs.baseSlots` (1), and what each of `slotGivers` he holds
  adds: a first class one, a second calling two, the Harrow relic one; and the `slots` of every
  level of the town that stands (the mill loft, the reading desk: one each).
- **Lessons** (`train.<kind>`, twice, listed while he has one of the kind): a month of what the
  kind eats, and the stat `more.<stat>` one more, which `feedFamiliars` adds to what each of them
  makes.
- **Kinds learned** are the world's (`know.fam.<kind>`, kept in the save's knowledge from life to
  life): `research.imp`, `research.brownie` and `research.weaver` (from 14, Letters 2 and Lore
  1) in *Study*, and the Old Shell (mole) and the Old Grey
  (weaver) and the Lantern Mother (wisp) among a boss's `gives` (`expeditiontable.pg`). A level of
  the town that `teaches` a kind makes it bindable while it stands, as if the world knew it, and
  its study is no longer listed (`kindKnown`: the smithy's bellows the mole, the chapel primer
  the imp).

**The stash** (`town.pg`, `Scenes/lifesave.cpp`, `Scenes/lifescene.cpp`, `activitytable.pg`). The
world keeps what a life held of each basic at its death (`LifeSave::stash`, written by
`beginAgain` after the gift to a place has taken its share), up to `stashMost` (`lib.pg`:
`economyKnobs.stashMost`, 100, and the `stash` of every level of the town that stands)
of each: `town.pg`'s `stash` output, `[{stat, most}]`, says what it keeps and how much. The
scripts read it as `stash.<id>` in `character`, the way they read a place's fund. A new life
draws on it with the rows of *The stash* (`draw.<id>`, at once, listed while it holds any; once
the town is known they stand on the Town page, under the Storehouse, by their `place`): as
much as he has room for, and the rest stays. Gems and what walks find that is not a basic are not
kept. A save from before the stash opens with it empty. The tile's caption says what the stash
holds ("40 IN THE STASH"); its price, what a draw takes ("30 firewood from the stash"). The ending
says what goes into it, after the story (`town.pg`'s `stashLine`, or the chosen place's when a gift
is chosen: "His stores go to the family's stash: 25 water, 12 stone."); choosing a place rewrites
the line in place.

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
name (the save's), `cause`, `text` and `tally`, and one Seal button. Under the button, at an
old man's ending only, one small muted line (`endAsk`, the script's `ask`) asks who played for a
comment or a rating on the game's page: plain words, no link, since the page is the one the game
is framed in on every host. `ask` is `""` for a life that failed before `lastAge`, and the scene
then takes the line out of the leaf. The page under it is
published once more, so it shows the life as it ended. The veil takes the mouse (hover is
topmost-only, so no row or button under it is hovered or clicked), and the scene refuses months,
confirms and `SPACE` while `ended`. *Begin a new life* (tag `life.again`) or `N` removes the
ending and starts the next life. A save written at an ending opens on it. If the ending cannot
be built, the next life begins at once, as it did before there was one. The words are table
driven in the script: `standings` (the highest flag he holds is the one said), `legacies` (by the
coin he held), the work he went back to most (terms of timed activities, the table's order on a
tie) and the deeds by name.

**Old age** (`lib.pg`, `resources.pg`, `activitytable.pg`). The last milestone (`lastAge`, 30) ends
his prime, not his life: its `entry` is written and the months go on. Every activity closes there
unless it says otherwise (`finishBy` defaults to `lastAge`); the *Old age* group and the market's
rations carry `finishBy: anyAge` and stay. The group is listed from `lastAge - 1`: the last
months of his prime are too short for any other work, and the months only run while he works.
`aging [{stat, fromAge, amount, every}]` takes `amount` from the stat's cap every `every` months
past `fromAge`, and from the stat when it stood above: nothing mends it back, so a life always
ends. It is not in `month.hurt`: no red, no pause. The clock's track stops at 30; its age
figure does not.

**Deeds** (`achievements.pg`). Each deed asks facts the Life scene publishes (`stat.<key>`,
`done.<activity id>`, `life.age`, `life.terms`, `life.working`, `working.<activity id>`, `life.lives`, `life.guide`) with
an `op` of `>=`, `>`, `<=`, `<` or `==`. An ask may be one that comes and goes (rations at 3 or
fewer): the entry is reached the month all of its asks hold at once. The scene hands them to the engine's `AchievementSys`
(`src/Engine/Systems/achievement.h`); when one is reached the scene applies `gives`, writes
`entry` in the log and keeps the id in the save.

**His sixth year** (`activitytable.pg`). The year's works are rows like the others, at the
head of the table, with ages counted to the month and a few optional fields:

- `fromMonths: N`, `finishMonths: N`: months past the `fromAge` and `finishBy` birthdays. The
  three works of two months are `fromAge: 6, fromMonths: 4, finishBy: 6, finishMonths: 9`: begun
  at 6y4m and 6y6m, two of them fit, and the third closes.
- `unless: "id"`: its row is not listed once that activity is done in this life (the wheel and
  the scribe name each other).
- `closingShown: N`: its closing is said from N months before and not sooner (-1: never), so
  that the year's short works do not count down a whole year.
- `showAfter: N`: listed once N terms in all are done in this life: the gleaning, the flour,
  the choice and the tome come one at a time, in every life (a first one has the guide to walk
  it through them). The rest of the year is listed by age, and the town's work has no
  `showAfter` any more: it is whole the day he is seven.
- `follows: "id"`: listed once that activity is behind him: done once in this life, or closed to
  him for good, so that nothing waits on what he missed. The works of a class come this way, a
  few at a time (the Keep's patrols after its service, the tourney after a patrol).
- `idleBy: N`: its row leaves the list the month he is N if he has never done it in this life,
  and nothing is said of it: the town's work a child never took. What he did once stays.
- `event: "id"`: the events of a life (`lifeEvents` in `lib.pg`: the Flood, the Levy, the
  Plague Year, the succession), each with most of a year to begin an answer. An answer brings
  one of the event's flags. The month an event is settled, `resources.pg` gives who holds none
  of them its `stood` flag, one more `stood_by`, and a line in the log: the rows the event names
  are shut (`barred` on the row, and its door on the clock), and nothing else: no number of
  events stood by shuts a class or a second calling. Only the month itself counts: a life
  begun later with `--age`, or a save from before the events, has stood by at nothing.

What is done in town (a `place`, a `raises`) is his from six, so that the market sells to the
boy who has just been taken there; every other activity asks 7 unless it says otherwise.

`freshLife()` (`Scenes/lifesave.h`) is a life's first frame, the first of a world or a later
one: 6 years old, 4 of each part, 35 of Vitality, no
`rations` stat at all (so no hunger and no row until the first task brings them), an empty
Inventory, the town not known. `boyOfSeven()` is the same boy past his sixth year without
having lived it (6 of each part, 40 of Vitality, 12 rations, nothing done), for `--age` and the
tests of what the town's work does.

**The town** (`towntable.pg`, `town.pg`). The town is the world's, not the life's: the save
keeps it beside `world` and `lives` (`town` place -> level, `raisedBy`,
`fund`, `seen`) and `newLife` carries it over. Its page is each life's to open
(`LifeSave::townKnown`, which a new life does not take from the last: *Go to the Market* gives it). The rules read it in `character`, with his stats,
under four names: `town_known` (1 or 0), `town.<place>` (its level), `fund.<place>` (the coin
left to it) and `seen.<place>` (1 once a life has had the place on its page). What a script hands back under those names is never written among his stats
(`LifeSave::takeTown`); the one thing a script does to the town this way is to make it known.

- `towntable.pg` is the table: eleven places, three levels each, a level being
  `{work, built, months, coin, costs, years?, cheaper?, gives, start?, slots?, teaches?, stash?, more?, lore}`.
  It is imported, never loaded. `costs` are mainly resources: a first level asks two basics
  (`economytable.pg`), a second more of them and gems, a third more again, gems and a thing
  found on a walk (`expeditiontable.pg`'s `walkItems`: pelts, hides, fangs, a white antler, bog
  iron, wisp-glass or a boss's trophy); `coin` is a small part of the price. The lodge is built of
  what walks bring (firewood, ash wood, clay, stone). What a level gives the economy, while it
  stands (`lib.pg`'s `levelsReached`): `slots: N` more familiar slots (`slotsOf`), `teaches:
  "<kind>"` a kind bindable as if the world knew it (`kindKnown`), `stash: N` more of each thing
  kept in the stash (`stashMost`), `more: [{stat, amount}]` more of that stat from each familiar
  that makes it (`moreFromTown`, read by `feedFamiliars`).
  A place may say what brings it onto the page: `shows: {flag: "veteran"}` (a flag he holds),
  `{done: "smithy"}` (an activity he has done a term of), `{lives: 2}` (a life of the world),
  `{fromAge: N}`, `{stash: true}` (the stash holds anything: the Storehouse), or several; without
  it, it is there from the day the town is known (the Market
  and the Mill alone). `lib.pg`'s `placeShown` weighs
  it for the page (`town.pg`'s `shown`) and for the works (a place not on his page has none
  listed), and a place raised, left coin or seen by a life before is always shown.
- `town.pg` (`Rules::town`) is what the Town page shows: each place's level, what it gives
  (`gives`, the last level's; `line`, its tile's, from the level's shorter `note` when it has one;
  `given`, every level reached; `tiers`, every level
  reached or not, for its gloss),
  and its next level flattened (`nextWork`, `nextCosts`, `nextCoin` less the fund, `nextGives`,
  `nextLocked`, `gaps`), plus `start` (what a new life is born with, every level reached),
  `leaves` (what he holds of the things its next level still asks: only things, never a part, a
  skill or a standing) and `spends` (what the fund pays of the next level; the scene takes that
  much from the fund when it is reached, and what is over stays for the level after), and the
  words the scene says (`opened`, `raisedLine`, `epitaphLine`, `fundLine`, `gift`).
- `activitytable.pg` makes the 33 works from the same table (`raise.<place>.<level>`): listed
  only to who knows the town and while the place is one level below. They have no row: a click
  on a place's tile on the Town page chooses its next one (`LifeScene::onPlace`), as a tile of
  the lists is chosen, and the button under "At work now" begins it, or a second click within
  400 ms, as on a row.
  New optional fields of an activity: `place` (it stands on the Town page, at that place, once
  the town is known), `goto` (confirming it opens a page and does nothing else), `raises` and
  `level` (its term raises that place), `years` (years of his life taken at once when it
  begins), `whileNot` (not listed once he holds that stat).
- Gear (`economytable.pg`'s `gearPieces`): `activitytable.pg` makes a `forge.<piece>` row of
  each, at the smithy (`place: "smithy"`, once a life), asking the smithy's level its `forge`
  names, and listed only with its `onlyWith` (the charm: `know.move.light.bite`, set the first
  time a marsh light's working reaches him). It gives `gear.<piece>`, a holding of the group GEAR;
  `fightcalc.pg`'s `gearSum` adds its `might` to his Might and takes its `armour` off each blow a
  beast lands, its `ward` off each working. Gear is never kept in the stash nor left to a place.
- How a level's gift reaches the rules: `bonus` is now a list of steps
  `{stat, needed, gains? months? rank? each? uses? fromAge? showAge? showFrom? finishBy? board? path? more?}`,
  every step that holds applied in order (`more` adds to what the term brings); `eased` takes
  an `at` level and may be a list; a requirement on `town.<place>` lists a new row; `start` in
  `towntable.pg` gives a new life a stat or a holding; and a holding of `resources.pg` with
  `ledger: false` works with no row of its own (the market hall's ration: `every: 3`, one
  month in three of his life, so the hall helps and rations still have to be bought or earned).

The scene's part: `takeStats` leaves the town out of his stats, `openTown` and `raisePlace` do
what a term did to it (a line, its lore, a toast, the page), `giveYears` moves the clock,
`showTown` swaps the middle column, `fillTown` / `layTown` / `publishTown` fill the `PlaceGrid`
with the places on his page and the glosses `place/<id>`, `seeTown` keeps a place shown for the
first time in `LifeSave::seen` and says the ones that come after the town itself, and `fillGifts` / `chooseGift` are the gift at death on the ending's leaf.

**The walks** (`expeditiontable.pg`, `expedition.pg`). A walk outside the town is asked of one
script, `expedition.pg` (`Rules::walk`), which keeps nothing between two runs: the walk as it
stands (`WalkState`, `Core/walk.h`) is handed in and handed back at every step, with the dice it
rolls (`run.seed`, so the same walk asked twice is the same walk). Its functions stand in two
files of their own, `expeditionlib.pg` and `fightlib.pg`, imported first: a script holds about
120 functions at most, each ahead of its tables (see *Every function lives in `lib.pg`* above).

- `expeditiontable.pg` is the table, imported and never loaded: `walkKnobs` (every figure of the
  rules), `walkItems` (what a walk gathers, uses and brings: `food`, `remedy`, `sold`, `town`),
  `walkZones`, `walkMobs`, `walkEvents`, `walkTechniques`, `walkStances`. Four zones for now:
  the Hedgerows (no beast), Mill Pond (one), the Hollow Woods (a pack, a boar, a rare stag) and
  the Quarry Road.
- **Days.** A stay is `stay x apPerDay` action points. Seeking a beast costs
  `max(1, base(rarity) - Guile/5 - zone knowledge/2 - familiarity + pack or lone)` (base 4, 6
  and 10 for common, uncommon and rare), a study 2, a
  watch 1, a rest 2 and a ration (+40 % of his Vitality), a remedy nothing, leaving nothing (asked
  twice: the first press says what is lost). His Vitality on a walk is
  his own: one count, the points a blow takes, at home, on the site and in a fight (a boy has
  40; a month with nothing to eat takes 5, and 5 come back every two months).
- **A fight** is a timeline (`fightlib.pg`): everyone waits `waitFor(weight, speed)` between two
  actions, `weight` percent of an ordinary move, his speed his Dexterity, a beast's its `quick`
  (3 more while a `leader` of its fight stands). The one with the least wait acts, he first on a
  tie; a quick move (a thrust, 70) holds him less, a heavy one (a finisher, 150) more. The timeline
  (`timelineRows`, as it stands and for each move of the menu; drawn by `LifeScene::fillTimeline`,
  the move under the mouse chosen each tick) runs from now to past the end of his day. Every `turnsPerAp` of his actions (3, one
  more for every 10 Dexterity, one fewer under half his Vitality) a day is gone, and a day begun
  is a day spent. A pack that holds back attacks with its first `holdsBack` members of a kind
  only. A fight may hold beasts of several kinds (`walkEncounters`, sought as `enc.<id>`). A delay
  (`delayFoe`) pushes a beast's next turn back, less each time on the same beast and half on a
  boss; no beast is held for good. Handed over, the fight plays itself (`fightauto.pg`'s
  `autoChoice`: a remedy under 30 %; the leader, else the weakest; a brace or a defend on a big
  blow coming when hurt; a full momentum spent; a working that sets off the mark on the target;
  a sure kill; a first element; then the move worth most for its time).
- **What a blow deals** (`fightcalc.pg`'s `hitRangeOf`): his roll (Strength and Arms for a blow,
  Intelligence and Arcana for a working), his stance, the move's power (and its rank), his hits
  on that beast since its last turn (+15 % each), what it resists of the move's type (`resist`,
  percent, negative for a weakness, which Lore makes deeper; a finisher halves a resistance, a
  crit ignores it), then its `armour` for a blow (a pierce goes through) or its `ward` for a
  working. Seven types: cut, pierce, blunt, fire, frost, storm, holy. A blow aims with his
  Dexterity, a working with his Intelligence. On a beast: one element's mark at a time (burning
  hurts at its turn, chilled waits longer, shocked takes more), bleeding, sundered, exposed (his
  next hit is a sure crit), and `poise`, worn down by his hits (blunt twice, a finisher thrice):
  at nothing it staggers, its next turn pushed back and its guard open. A beast's `phases` change
  its figures as it is hurt and may call more (never past four in a fight); a beast with the role
  `flees` runs at its turn once under 60 %.
- **What he can do in a fight**: anyone strikes, defends, takes a remedy he holds, or flees. The
  rest (`walkTechniques`) is learned by a flag of his life (`knows`) and is all his in every
  fight, grouped on the page as blows, ways and spells. The four rhythms: the Keep's momentum
  (each blow that lands +1, at 3 a `finisher`, *Heavy cut* or *Sweep*, spends it; a defend costs
  one, *Brace* keeps it); the Collegium's reactions (a second element on a marked beast: fire and
  frost shatter, fire and storm flare on all, frost and storm freeze); the Hidden Hand's tempo
  (quick moves, more hits before its turn); the Greenwood's preparation (traps set on the walk,
  the *Bow shot* that costs no time as a fight's first action). The early tasks teach the first
  tools: the blade's *Slash* or *Thrust*, the smithy's *Hammer blow*, the yard's *Bow shot* and
  *Second wind*, the rooftops' *Step aside*, the Boys' *Dirty kick* or the Watch's *Brace*,
  letters' *Read the beast*, the shelf's *Spark* or the keys' *Blessing*, Mara's *Rime*, the
  smuggler's *Sand*, the levy's *Heavy cut*. A fire flask and blessed oil from the market are
  fight moves while held. A technique used often rises a rank (10, 30, 60 uses): +10 % power
  and a point of the part it `trains`. The stances (`walkStances`) are in the rules, not on the
  page.
- **Before a fight**: when he can do more than meet it in the open (Stealth, a trap set, a ration
  for a pack), an approach card asks how (`approach`): lie in wait (Stealth against the beasts'
  `awareness`), lead it into the snare, draw one off, or leave it be.
- **Bosses** are encounters with `boss: true` (the Old Pike, the Old Shell, the Old Grey, the Lantern Mother), listed
  at knowledge 4 with a warning: beaten once a life (`beaten.<id>`), their `gives` and `lore`
  given straight to him, their steps written `once` so a record walked again passes them by.
  Lost, he dies (`fell`, read by `resources.pg`'s death line, and the life ends on the walk).
  Fled, the walk ends and what it gathered is lost. Brought down by an ordinary beast, he is
  carried home without what the walk had gathered (`loseHaul`), and nothing worse.
- **What a beast means to do** this turn is said of each by the move's name alone (`targets`:
  `life`, `percent`, `intent`). Each beast names its moves (`walkMobs.<mob>.moves`: a `name` and
  a `line`), and the row carries the gloss of the one coming: `moveName`, `moveLine`, and
  `moveEffect`, what it would take from him, written only when `moveKnown`. A move that draws no
  blood is plain to see; one that does is known once it has been used on him (the world's
  `know.move.<mob>.<move>`, kept from life to life as all knowledge is) or once he knows the
  beast two of four. Until then what is coming is counted and not weighed (`duel.warn`). Its
  vitality is in figures at the same two of four, in words before.
- **A field work** is a tile with its name and its days, and a gloss: the row says a line on it
  (`about`: a thing gathered has its own, `walkItems.<thing>.gather`), what it costs in words
  (`costs`) and what it gives (`gives`).
- **A blow that found nothing** is marked in the account (`missed`), his or the beast's: the
  page lifts a *MISS* off the rule of the one it was meant for (`LifeScene::showTag`).
- **A press is the move** (`act.kind` `move`): the menu's rows carry what each would do (`text`,
  `against`, `hits`, `deals`, `costs`, `then`, `bites`, `foeAfter`, `odds`: `previewOf`), which
  the scene puts in the button's gloss, and nothing is chosen before it is done. Against a pack
  the beast aimed at is a button too (`aim`).
- **A turn is told in its time** (`LifeScene::tellTurn`, `runTelling`). The script plays a
  whole turn at once, and the walk is at once where the turn left it; the page lags behind on
  purpose. Each line the turn wrote in the account says whose blow it tells (`by`, `n`), his
  Vitality once it has happened (`hp`) and what is left of the beast he hit (`left`): the scene
  fills that one's rule (`heroCast`, `foeCast<n>`), then writes the line and moves the rule of
  who was hit: it flashes (`progress.fill.hurt`), falls in a short time, and what was taken
  lifts off it (`showHurt`). Nothing is pressed meanwhile. Under reduced motion the turn is
  shown at once.
  Breath is said only to who has learned something to spend it on.
- **Events.** After an action an event waits on (`trigger`), one of the zone's may come: its card
  is answered before anything else, and the answer is a step of the record. One of `once: true`
  comes once in a life, and a record walked again passes it by.
- **Knowledge** is the world's: a zone is known 1 with the first visit, then by its studies
  (3, 6, 10); a beast by the times it was watched (1, 2, 5, 8) or, more slowly, fought (1, 8,
  20), and one more for the man who has fought it himself. A beast is listed from the zone
  knowledge its rarity asks; from 2 its vitality, its bite and what it does next are shown; from
  3 its weakness and its drops; at 4 a fight with it costs a day less. A zone known 3 is stayed
  in longer, known 4 it is nearer.
- **A place opens a little at a time.** On the first visit a place offers three things (the
  Hedgerows: berries, firewood, a study), and each level of its knowledge one more (its
  `ladder` in `expeditiontable.pg`: a gathering by its item, a beast by its id,
  `<beast>*pack` for its pack, `camp` for a rest and his snare and herbal; an entry with `says`
  is what the level opens elsewhere); what is not open has no tile, and the
  head's note says what the next level opens (`openingAt`); the study's gloss says only what it
  gives, `+1 zone knowledge`. The whole ladder, every level with
  what it brings and the studies it asks (`levelBrings`, `addLadder`), is said under the gloss of
  the place's tile on the Adventure page and in the gloss of his panel on site, whose rule fills
  with the studies toward the next level (`studyPercent`), not with the level he has. Known 2 of 4, the Hedgerows open a task
  of his life, *Explore the Nearby Pond* (`pond`, a month), that finds the pond for the world as
  the market finds the town: its gain is world knowledge, `know.found.millpond`, which Mill Pond
  asks, and no later life is offered it again (`whileNot`). Known 4 of 4, a place opens the next one:
  the Hedgerows open the Hollow Woods, Mill Pond the Quarry Road. The Hedgerows have no
  camp (home is a day away); the places with beasts have one from knowledge 2. A remedy has a
  tile only while he holds one and is hurt.
- **What a walk is worth.** A walk is months of his life like any work (the Hedgerows one, Mill
  Pond and the Quarry Road two, the Hollow Woods three: the longer, the more days on site), and
  is weighed against the work of his age for as long, in coin and its worth (a point of a part
  about 5 coin, a thing for the town half a coin). Known 1 of 4 a
  place brings about what his age's plain work does (a child's about 2.5 a month, a youth's 3);
  known 4 of 4, worked at its best task, one and a half to twice that, or a quarter more than a
  work that asks something of him (a child's about 3, a man's 4.5): about 4 a month at the
  Hedgerows and Mill Pond, 5 to 6 in the Hollow Woods and on the Quarry Road. Sale prices (`lot`
  for `coin`) and gatherings are set for that, and were
  measured by playing each place forty times a level with the rules themselves. Gatherings and
  the smaller beasts' spoils were then raised by about a third, by hand and not measured again:
  a walk also spends its days on study, for the knowledge that opens the rest. Since 0.7 a kill
  trains no part of him (the first of a kind in a life is a lesson instead, `lesson.<mob>`, and
  the beast is known 3 of 4 from then on), and hides, pelts and fangs drop one more at most.
  Since 0.8 every beast but a story's practice dummies leaves a chronicle page, sold four for a
  coin: measured the same way, a Mill Pond rat known 4 of 4 stays inside the rule, a wolf of the
  Hollow Woods is a little over it (6.5 a month), and the boar was over before the pages.
- **The record.** Every step is `{n, kind, target, count, ap, hp, text, gave, used, figure, ...}`
  and has its `marks` (`{step, kind, id, n}`: `use`, `gain`, `loot`, `kill`, `learn`, `stat`). A walk by the record is a fold over them (`replayTerm`): what it used is taken as he
  sets out (or `blocked` says what the bag lacks), what it gathered is gathered, what its beasts
  dropped is rolled anew, what it taught is taught, and his Vitality moves by what each step
  cost. A walk he was carried home from leaves no record.
- **A walk on the Life page.** `activitytable.pg` makes one activity a zone (`walk.<zone>`, the
  *Adventure* group, its field `zone`), its months `2 x travel + stay` in whole months
  (`lib.pg`'s `walkMonths`, the one figure both scripts read), and its `requires` what the zone
  asks: the task that opens it (`bow`), a stat, a place of the town (`town.lodge`), the place
  before it known 4 of 4 (`know.study.<zone>`, 10 studies) or what a task of a life found for the world (`know.found.millpond`). It is
  listed two years before he may go, locked. What is `sold` has a row at the market
  (`sell.<thing>`), wortleaf is brewed into remedies, and a new place of the town, the
  **Hunters' Lodge**, is built of firewood and ash wood: it opens the Hollow Woods to every
  life, then lengthens a fight's day, then finds every beast a day sooner.
- **What is for the town** (`walkItems` of kind `town`) is a holding like any other
  (`resources.pg`, the *STORES* of his Inventory), a cost of the works that raise a place, and
  lost at his death. `town.pg` says for each place what he holds of what its next level still
  asks (`leaves`, `leaveLine`): the ending offers the places that can use his coin or any of
  it, and the one he chooses keeps that much, under `fund.<place>.<thing>` (`LifeSave::fund`,
  key `"<place>.<thing>"`), counted toward what the level asks as the coin left to it is.

The scene's part is `Scenes/lifewalk.cpp` (LifeScene's own functions, in a file of their own):
`beginWalk` asks for the walk and opens its screen, `walkDo` hands every press to the script and
takes its answer (`after` becomes the character, with what the world knows:
`LifeSave::takeTown`), `fillSite` and `fillFight` write the answer on the screen by name,
`endWalk` keeps the record and puts the walk's months on the calendar, `fillAdventure` shows the
places (`zones`, a `PlaceGrid` as the town's: a tile each with its mark, a seal for each level he knows it to and a line, from the rules' `places`; a click sends `PlaceSelectedEvent` "walk.<zone>", which `onPlace` hands to `chooseZone`; `adventureZone`), the
button that sets out for the one chosen (`setOutAnew`), the walks kept of it (`records`, at most
five a place, sorted by the rules' `worth`; a line's own button is `walk.run.<id>`), the form
of a queue (its figures are the rules' `addFigures`) or, while one runs, the panel of the walk
at work, and the scribe's line at the foot (`walkZones.<zone>.lines`); `layAdventure` gives what
stands beside a button the room it leaves. `beginReplay` / `homeFromWalk` walk a record again.
The lines are the kit's list rows made table lines: a row with no glyph has no mark, its name
in the text's face, no hourglass by its cost, and its ground tinted when it is the one chosen
(`ActivityRowSpec::line`, `eachTone`, `asideTop` / `asideBottom`, `action`); the list is
`framed`, and has its columns' names inside its frame over its lines (`headName`, `headCost`,
`headAside`). The first thing to do is a button in ochre (`ButtonVariant::Ochre`), its months
in its label. The
save keeps the walk under way (`LifeSave::walk`), so a game closed in the middle of a fight
opens on it. The two screens are `res/chronicle/ui/expedition.yaml` and `fight.yaml`, built at
the window's width (`shapeWalk`); under 1000 px they are
`expedition-narrow.yaml` and `fight-narrow.yaml`, the same pieces in one column. At another
width in the same layout nothing is built: `sizeWalk` gives each panel its width where it stands
(the same figures as `shapeWalk` wrote in the file), and `LifeScene::widen` gives what stands in
a panel the width it leaves, each piece in its own way (`setWidth` on a label, a rule, a ledger,
a log, a stat line, the clock; `setSize` on a list, whose tiles are laid again without being
made again). The screen is built again only across the 1000 px line, three columns to one, and
then no more often than every 120 ms (`walkCooling`). The phone page is the same: `fitPhone`
gives its panels their width, and it is built again only when the page stops being the phone's.
The account of a fight has what the order of the turn leaves of its column (`fitAccount`).

A list given its rows again (`ActivityList::setRows`) does not build what it holds: a row that
would be built the same is kept, moved if it only changed place, and keeps the selection; the
others are made or let go of. So a term that ends makes again the tiles it changed, not the list.

**The stories** (`storytable.pg`, `storylib.pg`). An activity with a `story` (the id of a table
of `walkStories`) is not worked month by month: beginning it opens a scene of days on the walk's
screen (`LifeScene::beginStory`), played once in a life, never kept as a record, never walked
again and never on the Adventure page. The Collegium's term is the first: `collegium` has
`story: "collegium"`, `whileNot: "dropout.collegium"` (who dropped out is not listed again) and
no gains of its own, since the story gives them. The outskirts are the second, and every life's:
`outskirts` (Work, a month from 6 years 11 months, once, `whileNot: "outskirts.begun"`) opens
`outskirts`, four days that end the day he is seven: his lunch (6 rations), one of three paths
(each `not` the other two: Dexterity and *Step aside*, Intelligence and *Read the beast*, or
Strength and the *Stick blow*, the flag `outskirts.<path>` that the move `knows`), the path's fork
(`onlyWith` that flag: Dexterity or Lore, Intelligence or 2 remedies, Strength or Dexterity), and
the card at the ditch, whose one answer is a fight with the slime on the fight screen (it spares
him, and leaves four pages when beaten, the market's lot: every other beast leaves one). Its
script shows the pattern for a story's beast that teaches the beast's next move: a harmless move
(`circle`, here *Ooze*, which may say its own `verb`) then its one blow (`bite`, *Slap*), so the
panel's word for what is coming means something. A story whose name is a plural says so
(`plural`: "The Outskirts are behind him"). A story's `begun` is the flag entering it sets, so a story
left halfway is not offered again; who leaves keeps what the days gave. The script is still
`expedition.pg`, asked
`enter` with `act.kind` `"story"`; the functions that play a story stand in `storylib.pg`
(imported after `encounterlib.pg`, ahead of the tables: `expeditionlib.pg` is full), and the
stories in `storytable.pg`, whose header is the format: a story's `days` and `months` (its
row's), its `stages` (the head of the page), its `tiles` (the day's work: `days` it is offered,
a `fee` paid when taken, `needs` shown shut till met, `onlyWith` not shown till held, `not`,
`most`, `gives`, a gift with `when` landing once it holds, a `fight` fought on its own or on
the fight screen), its `cards` (on their day, or when their `when` holds; an answer may end the
scene), its own chance `events`, its `tests` (a card or a fight, scored from his stats, marks and
flags), its `grades` (`floors` for each test, a `total`, what each gives), its `gate` (the
courses that must be followed by its day, else its card sends him away) and its `standing`
(how he stands, in words: a line that promises a pass is not said while a test would fall under
its floor). The run is flat, as a walk's (`run.day`, `run["m.<mark>"]`, `run.monthsRun`); the
rules own both month counters, and each day's end says how many months fell due
(`run.monthsDue`), which the scene runs as the life's own (`runStoryMonths`: eaten, made, aged,
paid, written once they have all run). A stat a tile gives is his when it is taken, so who leaves
keeps it: the fire course's Firebolt is a drop-out's too. Left, sent away or failed, he drops out
(`dropout`, the flag of the story, which the Collegium's door reads: `windows.pg`'s `left`);
passed, the row's `enters` makes him one of its path (`endStory`). The page says what the days
are worth over the story's name, the stage's title over the gauge and its line on its first day,
a tile new today in the walk's vermilion and "No longer offered: ..." under the work. A story's
`plan` is its way through for the solver (`walks.pg`'s `storyBy`) and the tests (`planRow`, asked
`"plan"`). Two test stories, `trial` (its row `story.trial` is hidden behind `test.stories`) and
`drill` (no row), are for `test/chronicle/story.cc`.

**The guide and the lore** (`achievements.pg`, `kind`). An entry of the table is a deed (no
`kind`), a step of the guide or a line of lore; the `AchievementSys` watches the asks of all
three, and the scene does what the kind says (`reachDeed`).

- `kind: "guide"`: `say` is the sentence written on the guide's leaf (plain, easy English, said
  to the player as "you"; the leaf writes it in the `body` style, no smaller than its buttons), `point` what the hand stands
  beside (`button:begin`, `tile:<activity id>`, `holding:<id>`, `stat:<key>`, `tab:<page>`,
  `clock`, `log`, or `""`; on a walk's screens, a story's too, `work:<tile>`, `option:<n>`,
  `move:<id>`, `foe:<n>`, `order` or `walkbutton:<name>`: such a step is said only on its screen,
  and a Life page step waits under a walk), `until` the asks that end the step, `hold` the milliseconds it lasts when it has no
  `until` (5000 by default). A step that points at a tile may carry `pointChosen` and
  `sayChosen`: what the hand points at and what the line says once that tile is the one chosen
  (`button:begin`, and that a double click begins it too); let go of, the tile has the hand
  again. `order` 1..n is its place: the script adds the ask
  `life.guide == order - 1`, so a step waits for the one before it to have ended, and refuses a
  gap in the orders. `order: 0` is a step outside the sequence (`guide.rations_low`), kept in the
  save like a deed. A step outside the sequence with `world: true` is said once in a world, in
  whichever life meets it, and kept in `LifeSave::told` when it has been said; the fact
  `said.<step id>` lets another step wait for it (the town's three: its tab, its market, its
  places). Steps are neither toasted nor logged. Only a first life registers them, and
  only the ones past `LifeSave::guide`, so a reload resumes at the step it was on.
- `kind: "lore"`: `entry` is written in the log as a `LogKind::Lore` line (the `gloss` style,
  `ink-muted`, a quill, the theme element `log.lore`) and nothing else happens. Every life reads
  them, each once.

The scene's part is `runGuide` (`lifescene.cpp`): one step is said at a time, the others wait in
`stepsDue`; `pointed` finds what a `point` names on the page as it is now, `pointAt` hangs the
hand (the `manicule` mark) on its left edge, `shadeAround` lays the shade (`guide.veil`) over
the page but for it, and `sayNote` puts the leaf with the sentence under it (over it where the
window ends, at the head of the page when the step points at nothing). `nextStep` and
`skipGuide` are the leaf's two buttons; a skipped guide is saved as past its last step, with
the step it was left at in `LifeSave::guideSkipped`. What is not drawn (a tile not listed yet, a panel the
compact page keeps for another tab) is not pointed at: the sentence is said alone. Lore that
already holds when a save is opened is taken as read (`takeAsRead`), so a life from before 0.3
does not open on a page of it.

`Rules` loads them once from a root (`examples/Chronicle/rules` from the repo root, where
the game runs; `rules` beside `test_chronicle`, where CMake copies them) and gives each a typed
call. The forecast runs once per month tick, not per frame; `rules_test.timing_ceiling` keeps
100 runs under 100 ms (each run rebuilds the whole activity table: about 0.55 ms). The compiled `.pgc` files land beside the scripts and are git-ignored.

### The dependency graph (`rules/depgraph.pg`)

A tool, not a rule: the game never loads it and the web build leaves it out. It reads the activity
table and the town table and draws what leads to what.

```bash
release/PgCompilerBootstrap examples/Chronicle/rules/depgraph.pg chronicle-graph.svg
release/PgCompilerBootstrap examples/Chronicle/rules/depgraph.pg keep.dot keep
dot -Tsvg keep.dot -o keep.svg
```

**The timeline** (no focus) is an SVG the tool writes itself, the years left to right:

- **A bar** is a task, from the month it can first be begun, as wide as the months it takes: two
  bars that share a stretch of the axis cannot both be done in it. That month is its age
  (`fromAge`), or the month the task that brings a flag it asks for is over, when that is later
  (Buy Silence, after the robbery). The line after a bar runs to
  the age it must be over by (`finishBy`). What it asks, takes and brings is written on it.
- **A frame** is a group, as wide as its tasks and as high on the page as the frames over its
  years allow: a childhood's groups stand beside a class's, not over them. The ways into a class
  come first, in gold; a task of a class has a stripe of the class's colour, and its group a
  frame of it. The last resort stands where the last of the other ways in closes.
- **An arrow** goes from the task that brings a flag (a title, a deed, a friend) to the task that
  reads it: black asks for it, dashed green is eased by it or takes it as one of several.
- **No arrow** for what is only a number to reach or to spend: the skills (a stat three tasks or
  more bring, `skillFrom`), what a life is born with, and whatever a task takes as a cost (coin,
  rations, reagents, favors).
- **The town** is a frame like the others, under the ways into a class. A place is a box that
  begins where the task that brings it onto the Town page ends (its `shows`: a task done, or the
  task that gives the flag it asks for; the task that makes the town known, for a place there
  from the start), with a purple arrow from that task. Its three works are bars end to end,
  each as wide as its months (and the years of his life it takes), over what each asks and gives.
- **No arrow comes out of the town.** A task a level unlocks (it asks for the level, or the level
  opens it to every class) has a purple chip before it, `The Market 3`, and stays at its own
  age: what the town sells is on the page from the start. A task a level only makes better
  says so on its bar (`town: The Mill 1`).

**A focus** (the id of a task or the key of a stat: `keep`, `arms`, `town.mill`) writes a Graphviz
file of the detail around it, with a node for every stat, and prints the tasks that feed it and
the tasks it feeds. `all` draws the whole detail at once.

**Loose ends** are printed without a focus: what is asked for and brought by nothing, and what is
brought and read by no task.

It stands in `rules/` because a module's own imports are looked for beside the script that is run.
It reads each task's `needs`, the requirements as `activitytable.pg` writes them (with `eased`
and `anyOf`), which the table keeps for it beside the flat `requires` the screen reads.

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
BELLMOOR") and each activity's closing. A first life (`--fresh`) begins at Year 0;
the mockup's stands at month 126.

**Feedback and feedforward.** What happens is shown and what would happen is said first, all of
it in `lifescene.cpp` over the widgets as they are:

- A tile chosen in the list, or a place of the town, puts a **Begin** button in "At work now"
  (`showWorkButtons`), in the place of the one that passes a month; a second click or Enter
  still begins it. What may be done while at work (a thing done at once, an answer)
  has it under the running row. A thing done at once is named by its button ("Do it now"; at the
  market "Buy", "Sell" or "Make", by the rules' `trade`), stays chosen for the next press, and has
  **All** beside it when it is made from what he holds or bought again (a sale, "Sell all"; a buy
  with no `uses` limit, "Buy all"; a making, "Make all": `repeats`, `doAll`, stopped by his coin or his room as one
  press would be, one log line for them all; the scene hangs it on Begin, as a name inside a
  panel is out of reach of the page's anchors); once it can be done no more its button stays
  where it was, greyed ("None left", `spentChoice`), until a press anywhere else, so nothing
  else comes under a pressing mouse.
- The mouse on a tile is a **preview** (`ActivityHoveredEvent`, `preview`): the ghosts on his
  parts, and "46 -> 52" ("6/60 -> 12/60") on the holdings it would add to (what it costs is said by its gloss, not
  on his purse). What is chosen says more than what the mouse is on.
- What has a most he can hold shows it in its row (`holdingText`): "12/60", "64/60" past it.
- A tile counts down the months he still has to begin it (`closesOf`, the rules' `closesIn`:
  all along for what a door of its own closes, in its last two years for what only his prime
  closes), and says nothing else. What it asks (an age, a stat, a cost in coin) is not written
  on the tile: its ground says he cannot do it yet, and its gloss says why, under COST and
  REQUIREMENTS. The room a store still has is not listed there while there is some.
- What is done at a place of the town (the market's stalls, the works that raise a place) is on
  the Town page and nowhere else: while the town is not known it has no tile at all.
- The Adventure page's tab comes with his first walk (`adventureKnown`); a tab not shown leaves
  no gap (`Tabs::lay`).
- The rules say what is **running out** (`warning` from `resources.pg`, "RATIONS FOR 2 MONTHS"),
  written under his Vitality, and a part the coming month would take from is in red before it does.
- A figure that rises lifts a **"+2"** beside it (`showGain`), the log's new line is **lit** for a
  moment (`EventLog::lightLast`), and a deed, a milestone or a new activity is a **toast** at the
  foot of the window (`toast`). These last a moment each (`passing`, run by the ticks), wait out
  the second a page takes to arrive (`quiet`), and do not move under `--reduced-motion`.

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

**The phone page.** In a window narrower than 600 (`PhoneBelow`: a phone's browser, held
upright) the page is `res/chronicle/ui/life-phone.yaml`: one column as wide as the window, the
work at hand always in view, then one row of tabs (Life, Stats, Items, Years, Log, Town) and
under it one thing at a time (`LifeScene::showPhone`, `phonePanel`): the choice, the town in its
place, or one of the panels the other pages keep beside it. The page is built at the window's
width (`shapePhone` writes it over the file's before the build); at another, `fitPhone` gives
its panels their width where they stand (`LifeScene::widen`), and what is under the tabs the
height the work at hand leaves. Held sideways a
phone is wide enough for the compact page. What a touch screen needs with it:

- a tap is a click: the engine latches a press that is over before it is read (`Input::takePress`);
- a tap on a thing shows its gloss, which stays until the next tap (`TooltipSystem::showOnPress`,
  set on this page, and on any page once a finger has touched the screen: `OnSDLFingerDown`):
  a finger has no hover, and a tile says little without its gloss;
- the town's page scrolls under a finger (`dragToScroll` on its layout), as the lists do;
- the guide's hand goes to the tab that holds what a step speaks of when it is not the one open;
- the ending's leaf and the guide's are no wider than the window;
- the web page (`web/index.html.in`) gives the canvas the visible part of the screen (`100dvh`)
  and takes every touch for the game (`touch-action: none`, no zoom on a double tap).

z on the page: page 0, panels 10, their content 20–59, tooltips 200.

## Gate: phase 2

- **The mockup** (2026-09-30, `--no-save` at 1320 × 1020): the head (title, subtitle and age
  from the scripts), the tabs, the three columns in order with nothing overlapping and the
  page inside 1020, the clock's six ticks and five bands, two doors, the ledger's leaders,
  the stat lines with the Squire's threshold on Strength, the activity list with its locked
  Squire row, the log's rubrics and the skills all read from the save and the rules. Not
  compared side by side with the design system's MainLifeScreen yet: that comparison, and
  any component fix it asks for, is the first open item of the gate.
- **A later life** (`N` on the Life scene): age 7.0, WINTER, 84 months to the fourteenth; the ledger
  with his rations alone, the log one rubric and "Childhood", every door upcoming with what it asks. The
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

- The **mark** set (`res/icons/chronicle/`, 29 glyphs, the guide's `manicule` among them) and the **ornament** set
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

