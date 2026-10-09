# Chronicle changelog

What changed in the game, build by build, as a player sees it. The engine's own changes are in
the `CHANGELOG.md` at the root of the repository.

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
