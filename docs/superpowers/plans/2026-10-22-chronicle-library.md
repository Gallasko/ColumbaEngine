# Chronicle Chapel Library Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** After the first term of Learning to Read, a Study row opens the chapel library: a story of one act where you read one book (one month) or leave at no cost; books bought at the market stay on its shelf for every life after.

**Architecture:** The library is a story (`storytable.pg`) of one day and one month, with a tile per book and a new story flag `oneAct`. The shelf is a table in `lib.pg` that both the story's tiles and the row's rules read. A book read is a life flag `read.<book>`; a book bought is a world stat `know.library.<book>` (the save already keeps `know.` stats across lives).

**Tech Stack:** PgScript rules (`examples/Chronicle/rules`), C++ scene (`examples/Chronicle/Scenes`), GoogleTest (`test/chronicle`), built in `/release` (`make -j4 test_chronicle`).

**Spec:** `/docs/superpowers/specs/2026-10-22-chronicle-library-design.md` (draft 2)

## Global Constraints

- Every line the player reads speaks to them in the second person ("you"), plain and warm.
- The task *Learn Your Letters* is renamed **Learning to Read**; the stat stays **Letters**.
- Leaving the library without reading takes no month and loses nothing.
- A bought book stays on the shelf for every life after.
- Implementation calls (mine, the author asleep, to confirm): every book takes 1 month; the row is open while the shelf holds a book you have not read; *The Book of Well Spirits* replaces the row `research.imp` and is on the shelf from the first visit; *A Herbal Primer* replaces the row `herbal`; the weapon book gives Strength +1, the trap book Dexterity +1.

## Review Focus

- A life whose world already knows the well imp: the Book of Well Spirits is still offered and reads harmlessly; the row does not stay open on a shelf with nothing new.
- Leaving on the first press: no month passes, no "left before its end" line, the row is offered again.
- A book read is never offered again in this life, but is offered again in the next.
- A bought primer: the market stops selling it in this life and every life after; the library offers it the same month.
- An old save that held the keepsake `herbal_primer`: nothing crashes (the stat is simply unused).

---

### Task 1: The shelf and the row

**Files:**
- Modify: `examples/Chronicle/rules/lib.pg` (the shelf table and `libraryUnread`)
- Modify: `examples/Chronicle/rules/activitytable.pg` (rename `letters`; the `library` row; `buy.primer`; remove `research.imp` and `herbal`)
- Modify: `examples/Chronicle/rules/towntable.pg` (the chapel windows' line says Learning to Read)
- Modify: `examples/Chronicle/rules/resources.pg` (drop the `herbal_primer` keepsake row)
- Test: `test/chronicle/rules.cc`

**Interfaces:**
- Produces: `const libraryBooks = [{id, name, gives: [{stat, amount}], onShelf: "" | "<world stat>"}]` in lib.pg, ids `arith`, `saints`, `tales`, `arms`, `snares`, `spirits`, `primer`; `fun libraryUnread(character)` -> int (books on the shelf whose `read.<id>` the character does not hold). The row `library` with `story: "library"`.

- [ ] **Step 1: Write the failing tests** `rules_test.the_library_opens_after_the_first_letters_term` (row not listed before `done.letters`, listed and open after, `fromAge` 7, months 1, `story` "library"); `rules_test.the_library_closes_when_its_shelf_is_read` (all six plain `read.*` flags: not listed; with `know.library.primer` and five read: listed); `rules_test.a_bought_primer_goes_on_the_shelf` (`buy.primer` gains `know.library.primer`, not listed once held); `rules_test.the_library_replaces_two_studies` (no `research.imp`, no `herbal` row); `activityAt(... "letters").fields["name"] == "Learning to Read"`.
- [ ] **Step 2: Run them, see them fail.** `cd release && make -j4 test_chronicle && ./test_chronicle --gtest_filter='rules_test.*library*:rules_test.a_bought_primer*'`
- [ ] **Step 3: Implement.** The shelf (names and gives as the spec's table, the primer `onShelf: "know.library.primer"`); `libraryUnread`; the row (`requires: [{stat: "done.letters", label: "Learning to Read, once"}]`, `showFrom: 100`, `uses: 0`, about "Father Anselm keeps a shelf of books for anyone who can read them. Now you can."); a row with `shelf: true` is not listed while `libraryUnread` is 0; `buy.primer` gains `know.library.primer`, `whileNot: "know.library.primer"`, about "A few pages of leaves and roots, badly copied and sold dear. Once bought, it stays on the chapel library's shelf for good."; `read.` flags hidden from the ledger (lib.pg's hidden stats).
- [ ] **Step 4: Run the tests, see them pass; run the whole suite, fix the tests that pinned the old names.**
- [ ] **Step 5: Commit.**

### Task 2: The story of one act

**Files:**
- Modify: `examples/Chronicle/rules/storytable.pg` (the `library` story)
- Modify: `examples/Chronicle/rules/storylib.pg` (`oneAct`: leaving at no cost, the log line of the book read)
- Test: `test/chronicle/story.cc`

**Interfaces:**
- Consumes: `libraryBooks` (tile ids are the book ids; a test checks they match).
- Produces: story `library` {days: 1, months: 1, oneAct: true}; each tile `cost: 1`, `onlyWith` the book's `onShelf` when it has one, `"not": ["read.<id>"]`, gives the book's gives plus `{stat: "read.<id>", amount: 1}`, line "You read <name>".

- [ ] **Step 1: Write the failing tests** `story_test.the_library_reads_one_book` (enter, take `tile.arith`: Intelligence +1, `read.arith` 1, `monthsDue` 1, mode done, result over, the life's entries hold "You read A Child's Arithmetic"); `story_test.leaving_the_library_costs_nothing` (enter, leave once: mode done, result left, `monthsDue` 0, no entry written); `story_test.a_book_read_is_not_offered` (with `read.arith`: no `tile.arith`); `story_test.the_primer_waits_for_the_shelf` (no primer tile without `know.library.primer`, one with it); `story_test.the_library_tiles_are_the_shelf` (tile ids == `libraryBooks` ids).
- [ ] **Step 2: Run them, see them fail.**
- [ ] **Step 3: Implement.** The story table entry (title "The chapel library", stage text "Father Anselm's shelf, and a stool by the window. One book today, or another time."); in storylib, a `oneAct` story leaves on the first press, writes no record "LEFT" line and no "Left ..." entry; ending by its tile writes the tile's line as the life's entry instead of "Saw ... to its end"; the head says `oneAct` for the scene (`site.oneAct`).
- [ ] **Step 4: Run the tests, see them pass.**
- [ ] **Step 5: Commit.**

### Task 3: The life screen and the guide

**Files:**
- Modify: `examples/Chronicle/Scenes/lifewalk.cpp` (`endStory`: a one-act story left says "You put the books back", read says nothing more than its entry)
- Modify: `examples/Chronicle/rules/achievements.pg` (step `guide.library`)
- Test: `test/chronicle/lifescene.cc`

- [ ] **Step 1: Write the failing tests** `lifescene_test.the_library_takes_a_month_for_a_book` (a life of 7 with `done.letters`: begin `library`, take a book, a month passed, back on the Life page, the row still listed, the book not offered again); `lifescene_test.leaving_the_library_takes_no_month` (begin, leave: age unchanged, nothing running, the row listed); `lifescene_test.the_guide_points_at_the_library_once`.
- [ ] **Step 2: Run them, see them fail.**
- [ ] **Step 3: Implement.** The toast; the guide step (`order: 0`, `point: "tile:library"`, `hold: 6000`, asks `done.letters >= 1`, say "You can read now! Visit the chapel library and pick a book.").
- [ ] **Step 4: Full suite green; the solver still reaches the tasks it reached (`release/ChronicleSolver ... task library`, `task herbal` is gone, `task bind.imp`).**
- [ ] **Step 5: Commit; update the spec's "Calls I made" with what was built.**
