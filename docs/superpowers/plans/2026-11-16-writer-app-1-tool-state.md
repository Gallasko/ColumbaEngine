# Writer App, Part 1: the `article` Tool Says Its State — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `article state <slug>`, `article articles --json` and `article ideas list --json` print JSON the Writer app reads, with the next step of each article worked out by the tool.

**Architecture:** One new library file `/tools/writer/lib/state.pg` computes an article's state from its record, plan and files (`articleState(config, slug) -> table`); the commands print it with `json.stringify` on one line. `stitch` keeps a snapshot of the kept texts so `state` can tell a re-stitch is due.

**Tech Stack:** PgScript (`release/PgCompilerBootstrap`), the tool's libraries and test runner.

**Spec:** `/docs/superpowers/specs/2026-11-16-writer-app-design.md` (Part 1, and "Keeping a chapter" in Part 4).

## Global Constraints

- The JSON is printed as **the last line** of the command's output, compact (`json.stringify(v)`, no indent): the command-line tool prints its own banner first ("8 bytes per Value on this platform."), so readers take the last non-empty line.
- `next.step` is one of `plan`, `draft`, `pick`, `write`, `mine`, `stitch`, `share`, `done`; `next.args` the `article` arguments to run (`[]` for a step done by hand); `next.needsAuthor` true for `pick` and `write`; `next.label` a short sentence for the button or the hint.
- The tool's traps (see `/docs/superpowers/plans/2026-11-16-writing-tool-core.md` Global Constraints): libraries import native modules only; no two `fun`s of one name; a test file stays under 256 constants before its `fun`s; no string literal starting with a digit then `.`; results in a `var` before reading fields.

## Review Focus

1. A chapter file `NN.md` that is empty (just made by "Write my own"): `next` is `write n`, never `mine` (Task 1).
2. A kept chapter edited after a stitch: `next` is `stitch` again; untouched: `share` or `done` (Task 1).
3. A plan chapter added after a stitch: `next` is `draft` of the new chapter, not `share` (Task 1).
4. `plan.md` unreadable (a half-typed edit): `state` fails with the plan's error, exit 1, no JSON on stdout's last line (Task 1).
5. `articles --json` with a folder holding no `record.json` (a stray folder): skipped, not an error (Task 2).

---

### Task 1: `article state <slug>`

**Files:** Create `/tools/writer/lib/state.pg`, `/tools/writer/test/t_state.pg`; Modify `/tools/writer/article.pg` (import, dispatch, usage), `/tools/writer/lib/steps_publish.pg` (stitch snapshot), `/tools/writer/test/run.pg` (imports), `/tools/writer/README.md`.

**Interfaces:**
- Consumes: `openArticle`, `chapterFile`, `codeFile`, `voiceNames`, `recordChapter`, `fileText`, `articleDir` (existing).
- Produces:
  - `markerCount(text) -> int` — occurrences of `<!-- visual:` and `<!-- number:`.
  - `chapterState(config, slug, chapter, entry) -> table` `{n, title, code, files: {kept: bool, drafts: [voice], code: bool}, kept, edited, markers}`; markers from `NN.md` when it exists, else the largest over the drafts.
  - `nextStep(config, record, plan, chapters) -> table` `{step, n, label, args, needsAuthor}`, by the spec's order: per chapter in plan order — no draft and no `NN.md` → `draft` (`["draft", slug, n]`); `NN.md` exists and empty (trimmed) → `write` (needs the author, `[]`, label `"write chapters/NN.md"`); `NN.md` with text, `kept` none: equal to a draft → `pick` with that voice (`["pick", slug, n, voice]`), else `mine` (`["mine", slug, n]`); drafts but no `NN.md` → `pick` (needs the author, `["pick", slug, n]`, label "keep a draft of chapter n"); then `stitch` (`["stitch", slug]`) when never stitched or `published.texts` differs from the kept texts (by chapter number and text, or a different chapter set); then `share` (`["share", slug]`) when `posts` is empty; else `done` (`[]`).
  - `articleState(config, slug) -> table` `{slug, title, theme, chapters, published: {path, date}|none, posts: <count>, next}`.
  - `commandState(config, slug)` prints `json.stringify(articleState(...))`.
  - `stitch` additionally stores `published.texts`: `[{n, text}]` of the kept texts it wrote.

- [ ] **Step 1: Write the failing tests** (group `state`, in `t_state.pg`; each state read by `stateOf(site, slug)` = the last non-empty line of `runTool(site, ["state", slug], "")` parsed with `json.parse`)

```
// after plan: next.step "draft", n 1, args ["draft", slug, "1"], needsAuthor false; 3 chapters, files.drafts []
// after draft 1: next "pick" n 1, needsAuthor true, args ["pick", slug, "1"]; chapter 1 files.drafts [guide, story, brief]
// a draft holding "<!-- visual: chart -->" and "<!-- number: x -->": chapter 1 markers 2
// pick 1 guide: chapter 1 kept "guide"; next "draft" n 2
// an empty chapters/02.md (no drafts of 2): next "write" n 2, needsAuthor true, args []      (Review Focus 1)
// 02.md with text: next "mine" n 2, args ["mine", slug, "2"]
// 02.md equal to a 02-story.md draft, not kept: next "pick" n 2, args ["pick", slug, "2", "story"]
// all kept (via the commands), never stitched: next "stitch"; after stitch: "share"; after share: "done"
// edit a kept file after stitch: "stitch"                                                       (Review Focus 2)
// add a 4th chapter to plan.md after stitch: "draft" n 4                                        (Review Focus 3)
// plan.md with no chapter: state exits 1, last line is not JSON                                 (Review Focus 4)
// state of a missing slug: exit 1
```

- [ ] **Step 2: Run** `release/PgCompilerBootstrap tools/writer/test/run.pg state` — Expected: fails (`state` is not a command).
- [ ] **Step 3: Implement** `state.pg`, the `published.texts` snapshot in stitch, the command, the usage line, a README line.
- [ ] **Step 4: Run** the same — Expected: `N passed, 0 failed`.
- [ ] **Step 5: Commit** `git commit -m "Writing tool: article state <slug> as JSON"`.

### Task 2: `article articles --json`, `article ideas list --json`

**Files:** Modify `/tools/writer/lib/state.pg`, `/tools/writer/lib/ideas.pg` (`commandIdeasList(config, asJson)`), `/tools/writer/article.pg`, `/tools/writer/test/t_state.pg`, `/tools/writer/README.md`.

**Interfaces:**
- Produces: `commandArticles(config)` — for each folder in `writer/articles/` (`os.listDir`, sorted) holding a `record.json`: `{slug, title, next}` (`next` from `articleState`; a folder whose plan cannot be read gives `next: {step: "plan", label: "plan.md cannot be read", args: [], needsAuthor: true}` instead of failing); prints the list as JSON. `article ideas list --json` prints `ideas.json`'s value.

- [ ] **Step 1: Write the failing tests**

```
// articles --json with one planned article and a stray folder (no record.json): a list of 1, its slug and next.step "draft"   (Review Focus 5)
// an article whose plan.md is broken: listed with next.step "plan", needsAuthor true (the command exits 0)
// ideas list --json: the last line parses; its ideas count equals ideas.json's; `ideas list` without --json unchanged (has "new:")
```

- [ ] **Step 2: Run** `... run.pg state` — Expected: the new checks fail.
- [ ] **Step 3: Implement** both commands.
- [ ] **Step 4: Run** `release/PgCompilerBootstrap tools/writer/test/run.pg` (all groups) — Expected: all pass; the duplicate-`fun` check prints nothing.
- [ ] **Step 5: Commit** `git commit -m "Writing tool: articles --json, ideas list --json"`.
