# Chronicle changelog

What changed in the game, build by build, as a player sees it. The engine's own changes are in
the `CHANGELOG.md` at the root of the repository.

## 0.4 - 2026-10-10

### New

- **The town.** *Explore Bellmoor* (three months, once) opens a Town page beside the Life page.
  Its tile stands out in gold so you do not miss it, and the Town tab only appears once you have
  explored. A short tutorial then shows the tab, the market where your coin buys rations, and
  the places.
  It has nine places of three levels each, and it grows with what you do. At first you see
  two: the Market and the Mill. The others are found by living:
  - the Smithy, once you have worked a term at the forge;
  - the Yard, once you have joined the Border Campaign, and the Watch Gate, once you have
    taken the Captain's Chain;
  - the Chapel, once you have entered the Collegium, the Collegium Gate after assisting
    Magister Orin, and the Harrow Road after the expedition to Harrow;
  - the Inn, from the second life of a world.

  The day a place comes, the work that builds it comes with it. A place you have seen stays
  on the page for your next lives.
- **Works for the town.** On the Town page, under the places, a work raises a place by one
  level. Clicking a place picks its work for you. They cost months and coin, and some cost a
  stat or years of your life. This is time you do not spend on yourself.
- **The town stays.** What you raise is kept when a life ends. The next life is born into that
  town: tasks that are shorter or pay more, classes that ask a little less, and a few things you
  start with (rations, a skill point, a friend at the Watch). The town never gives a class for
  free.
- **The gift at death.** When a life ends with coin, you can leave it to one place. It counts
  toward that place's next level.
- **Hover a place** to see what it gives, what its next level asks compared with what you have,
  and who raised it.
- **A notice on a tab.** When something new comes onto the page you are not looking at (a new
  task, something new at the market, a new place), its tab shows a small red count until you
  open it. Only what was never on the page counts: a task you can afford again is not news.
- **The tiles that turn a life stand out.** The four classes and *Explore Bellmoor* have a gold
  frame and their mark in the corner, locked or not.
- **They say what they do.** Hover one of them: it tells you what it changes, with a word from
  the town.

### Changed

- **Buying is done in town.** *Buy rations* and *Buy reagents* are on the Town page, at the
  market. The Life page keeps a *Go to the Market* tile that opens it.
- **Quieter tiles.** A locked tile no longer writes what it needs under its name (an age, a
  stat, coin, room). Its tooltip still says all of it. A tile only says when it is about to
  close.

### Fixed

- Parts of the Town page were sometimes drawn over the Life page, and the other way round, when
  a tile changed on the page you were not looking at.
- A task begun by a double click could not be clicked again until the mouse moved.
- The tutorial could speak of the market in a new chronicle before the town was explored. It
  now comes the first time you open the Town page, on *Buy rations*.

## 0.3 - 2026-10-09

### New

- **A first life opens on one task.** One tile: *Start Helping Out* takes a month, brings you to
  7 and gives you your first rations. The other tasks join the list one at a time
  as you finish terms, and the three classes come with the fourth, when the guide speaks of them. The second life and the ones
  after it open with the whole list, as before.
- **A guide.** Through the first six works a short sentence under the thing it is about says
  what to look at, a pointing hand stands beside it and the rest of the page is shaded. *Next*
  passes a sentence without waiting for it, and *Skip tutorial* ends the guide. To begin a work it
  shows the task first, then the Begin button, and says that a double click on the task begins
  it too. Nothing is blocked:
  you can press anything at any time. It is said once, in the first life only, and a reload
  resumes where it was.
- **Lore in the log.** A few lines about Bellmoor are written in the log as a life goes by.
- **Help in the Kitchen.** A work that feeds you while you are at it and sends you off with
  rations.

### Changed

- **Buy rations is gone from the Life page until the town opens; the kitchen feeds him.**
- The welcome line of a first life is now the guide's first sentence.

## 0.2 - 2026-10-08

Everything since 0.1, the first build published on itch.io (2026-10-07).

### New

- **A guided start.** A new life opens with its first work already chosen: press Begin and the
  months pass. A line in "At work now" says what to do, and "Pass a month" only comes once a
  first work is done.
- **A word of welcome.** A first life is told in one line what the game is.
- **Classes.** Paths are now called classes. The three of them (the Keep, the Collegium and the
  Hidden Hand) stand in the list from the first day, locked, so you can see what a childhood
  leads to.
- **A fourth class: the Renegade.** Miss all three, and the month the last one closes "Take to
  the Greenwood" opens. It has its own works, a proving, a mastery (King of the Greenwood), two
  deeds and its own ending.
- **A Begin button.** Choosing an activity puts a Begin button in "At work now", with the months
  it takes ("Do it now" for what takes no time). A second click or Enter still begins it.
- **See before you choose.** Pointing at an activity shows what it would bring to your stats
  and to what you hold.
- **Gains you can see.** A figure that rises lifts a "+2" beside it, and the line just written
  in the log is lit for a moment.
- **Toasts.** Deeds, milestones and activities that have just become possible are said at the
  foot of the window.
- **Warnings before it hurts.** Rations running low are said from three months left, with what
  to do about it, and a stat the coming month would take from turns red before the month, not
  after.
- **Locked activities say why.** A locked tile names the first thing it still asks.
- **Limits in sight.** What has a limit reads "12/60" in its row.
- **Smaller windows.** The three columns fit windows down to 680 x 566 (1136 x 664 before), and
  the left column scrolls when it is too tall.
- **Plainer names.** The panels are Inventory, Stats and Log.

### Fixed

- A crash ("Segmentation fault") some players met.
- The Begin button always read "3 mo", whatever was chosen.
- The log lost its last lines when a work began.
- The "+1" of a gain stood a line too high when the same month added a row.
- The page could be drawn half placed for a few frames, and new tiles once without their
  ground. A month is also quicker to work out.

## 0.1 - 2026-10-07

The first build on itch.io: one life from 7 to 30 and past it, three paths, the web build.
