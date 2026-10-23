# Chronicle Collegium Rework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Collegium's course is chosen as the day's only tasks, the dummies are fought by hand, the exams are two tasks (a four-question quiz whose wrong answers your preparation strikes off, then the fight), nobody is sent away, and Vitality does not mend while a story runs.

**Architecture:** Story engine additions in `storylib.pg`: `sole` tiles (when one is offered, only sole tiles are), a tile cost cheaper with a stat (`cheaperWith`), tests opened by a tile (`opensWith`), and a test kind `quiz`. The Collegium's table in `storytable.pg` uses them. `resources.pg` skips mending while the stat `in.story` is held; the scene sets it while it runs a story's months.

**Tech Stack:** PgScript rules, C++ scene, GoogleTest; `/release`, `make -j4 test_chronicle`.

**Spec:** `/docs/superpowers/specs/2026-10-23-chronicle-collegium-rework-design.md`

## Global Constraints

- Every line the player reads speaks to them ("you"), plain and warm.
- A day is 1 AP; costs are said in AP.
- The quiz: 4 questions drawn from a pool, 4 answers each, wrong answers struck by preparation.
- Grades: pass = theory 2 right and practical 10; merit = theory 3; distinction = theory 4 and practical 20.

## Review Focus

- A student with no elemental lectures on day 7: only the basics can be taken (the three elements shown shut, saying why).
- The basics' 3 AP from day 7 run into days 8 and 9; with Intelligence 14, 2 AP.
- A quiz answer that is struck off cannot be chosen; the same life's seed draws the same questions.
- A story's months never mend Vitality; a day's rest still does.
- Old saves in the middle of a Collegium with the old card answered: the course mark holds, nothing breaks.

---

### Task 1: The engine — sole tiles, cheaper costs, tests opened by a tile, the quiz

**Files:** `examples/Chronicle/rules/storylib.pg`; test `test/chronicle/story.cc`

- Produces: tile fields `sole: true`, `cheaperWith: {stat, at, cost}`; test fields `opensWith: "<tile id>"`; test `kind: "quiz"` with `pool: [{id, q, right, wrong: [3]}]`, `draw: 4`, `strikes: [{at, n}]`, `score` (the preparation terms). Run fields `quiz.<test>.n`, `quiz.<test>.right`, `quiz.<test>.q<i>`. The card of a quiz is titled "<test title> · Question i of 4", its options `a0`..`a3`, a struck one locked with asks "STRUCK OFF".
- [ ] Tests (on the Collegium's table, Task 2): sole day 7; basics cost 3 / 2; the theory opens only by its tile; struck answers locked; 4 answers score the right ones; the same seed draws the same questions.
- [ ] Implement; run; commit with Task 2.

### Task 2: The Collegium's table

**Files:** `examples/Chronicle/rules/storytable.pg`; tests `test/chronicle/story.cc`, `test/chronicle/lifescene.cc`

- Day 7 sole tiles `choose.fire`, `choose.frost`, `choose.storm` (1 AP, need `elemental` 2), `choose.basic` (3 AP, 2 with Intelligence 14); the card `course` removed; the courses and the basics on days 8 to 14; practice fought by hand; tiles `theoryexam` (day 15) and `practicalexam` (day 16), sole, 0 AP, opening the tests; the gate and the card `sent_away` removed; grades as the constraints; standing on the new estimates; every line in the player's voice; a pool of 12 questions.
- [ ] Update the tests that walk the Collegium (plan tiles, the solver's `plan`), run, commit.

### Task 3: No mending in a story

**Files:** `examples/Chronicle/rules/resources.pg`, `examples/Chronicle/Scenes/lifewalk.cpp`; test `test/chronicle/lifescene.cc`

- [ ] Test: a life hurt in the Collegium has the same Vitality after a month of its days as before (but a day's rest +10).
- [ ] Implement (`in.story` held by `runStoryMonths`, mending skips it); run the suite; commit.
