# Chronicle: the chapel library (draft 2)

Status: built 2026-10-22 (commit ff88f7df), from draft 2 with the author's answers to draft 1.
The plan: `/docs/superpowers/plans/2026-10-22-chronicle-library.md`.

## As built (the author asleep: these are mine to confirm)

- **A book takes its own months** (the author, after playing it): 2 for a book of a stat or a
  skill, 3 for The Book of Well Spirits. A visit is a story of one act (`storytable.pg`'s
  `library`, `oneAct: true`); a tile's `months` is the time the visit takes.
- **The row asks Letters 1** (the author: Letters is how much you can read), and it goes once the
  life has joined a class.
- **The page** says "Pick a book to read", each book's months, and "If you leave now, you gain
  nothing, but you don't lose any time." (the author's words).
- **Leaving** before reading ends the visit at once: no month, no log line, no toast, no second
  press. The row stays.
- **The shelf** is `lib.pg`'s `libraryBooks`; the row is listed while one book on it is unread.
- **The weapon book gives Strength +1, the trap book Dexterity +1** (no Arms, no snare: the snare
  stays the market wire's and its task's).
- **The Book of Well Spirits is on the shelf from the first visit** (the old study opened at 9).
- **A save that bought the old primer** (the keepsake `herbal_primer`) keeps it as a keepsake; it
  is not on the shelf, and the herbal is learned by buying the new one. No migration.
- The guide says once, the month the library opens: "You can read now! Visit the chapel library
  and pick a book."

## Why

After the first playthrough of the mage arc the author found the early game short on things for a
reader to do: until 9, Learn Your Letters is the only study open. Learning to read should lead
somewhere at once. The author's idea: a library in Father Anselm's chapel, opened right after you
learn to read, played as a story walk where you choose a book. Books you buy go onto its shelf,
and stay there for every life after.

## What the player sees

1. **Learning to Read.** *Learn Your Letters* is renamed **Learning to Read** (the author's call).
2. **The row.** Once its first term is done, a Study row appears: **Visit the Chapel Library**.
   Its line: *"Father Anselm keeps a shelf of books for anyone who can read them. Now you can."*
3. **One choice.** Taking it opens a story walk with a single action: you pick one book from the
   shelf to read (one tile per book), or you leave.
   - **Reading a book** takes the time that book needs (1 or 2 months), and gives what it teaches.
     The record keeps a line for it ("You read *The Lives of the Saints*").
   - **Leaving** ends the walk at once: no time passes, nothing is lost.
4. **Read for this life.** A book read is read for this life: its tile is gone the next time.
5. **Coming back.** The row stays while the shelf holds a book you have not read.
6. **Bought books stay forever.** A book bought at the market goes onto the shelf, and stays
   there for every life after (the world remembers it, as it remembers the town). A later life
   reads it without buying it again.

## The first shelf

The plain books every life finds there:

| Book | Time | Gives | Replaces |
|---|---|---|---|
| A Child's Arithmetic | 1 month | Intelligence +1 | |
| The Lives of the Saints | 1 month | Discipline +1 | |
| Tales of Old Bellmoor | 1 month | Lore +1 | |
| The Sergeant's Manual of Arms | 2 months | Strength +1, Arms +1 | |
| On Snares and Deadfalls | 2 months | Dexterity +1, and a snare known | |
| The Book of Well Spirits | 1 month, from 9 | knowing the well imp | *Read of the Well Spirits* |

The first book to buy:

| Book | Bought | Gives | Replaces |
|---|---|---|---|
| A Herbal Primer | market, 4 coin, from 10y6 | the herbal (brew remedies on a walk) | *Learn the Herbal* |

## Books to come (the author's list)

Bought at the market (or found on walks), each one onto the shelf for good:

- **Familiar binding** books: the knowledge of a familiar kind (as the Book of Well Spirits).
- **Spells**: a working learned outside the Collegium.
- **Martial arts**: a new move or technique.
- **Lore** books: a flag that helps you ace the Collegium's exams.

Which ones, at what price and from what age, is a later batch.

## Calls I made (the author to confirm)

- **"Counts the 1/2 month it needs"** read as: each book has its own time, 1 or 2 months, and
  that is the time the visit takes. If you leave without reading, the visit takes no time.
- **The first shelf.** The titles and what each gives are mine (the author asked for weapon and
  trap books for Strength and Dexterity). *On Snares and Deadfalls* gives the snare that *Snare
  Rabbits on the Common* teaches now; whether it replaces that task is open.
- **Books replace the study rows they duplicate** (well spirits, herbal), so nothing is offered
  twice. The forbidden shelf and the Old Chronicle stay as tasks for now.
- **The primer**: buying it puts it on the shelf; the market row is no longer offered once the
  shelf holds it, in this life or any after.
- **Intelligence stays scarce.** Only *A Child's Arithmetic* gives Intelligence (+1): the most a
  life can reach by 16 goes from about 14 to about 15, and the Collegium keeps asking 12.
- Only the task's name changes to Learning to Read; the stat stays **Letters**.

## For the builder

- The story system already does what this needs (`storytable.pg`): tiles with `onlyWith` for a
  book that must be on the shelf, `"not"` for a book read, `gives` of stats and flags. A story of
  one action needs: a tile's own time in months (today a story's months fall due as its days
  pass), and a way to leave that costs nothing (no month falls due, no term counted).
- A book read is a flag of the life (`read.<book>`); a book on the shelf is a world stat
  (`know.library.<book>`): the save already keeps `know.` stats across lives.
- The row is an activity with `story: "library"`, `requires: [{stat: "letters"}]`, and is hidden
  once every book on the shelf is read.
- The guide may point at the row once, the first time it appears (a step like `guide.outskirts`).
- Every line is in the player's voice, as the copy rework asks.
- Tests: the row opens after the first term of Learning to Read; leaving costs no month; a book
  read takes its months and is not offered again in this life; a bought primer is on the shelf in
  the next life and is not sold again; the replaced study rows are gone.
