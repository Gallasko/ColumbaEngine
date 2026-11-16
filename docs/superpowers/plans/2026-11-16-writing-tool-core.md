# Writing Tool Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The `article` command-line tool in PgScript: ideas → plan → code → draft/pick → stitch → share, recorded per article, drafting with `claude -p` on the author's subscription.

**Architecture:** `/tools/writer/article.pg` reads the command and dispatches to step functions in `/tools/writer/lib/*.pg`. Every Claude call goes through one function that runs `claude -p` with `os.call` (prompt on stdin, a JSON schema, read-only tools or none) and returns the reply's `structured_output`; the tool writes every file itself. The author's material and records live under `<site>/writer/`. Tests are a PgScript runner that checks the pure functions directly and runs each command as a subprocess against a fake `claude` script.

**Tech Stack:** PgScript (`release/PgCompilerBootstrap`), the `os`/`json`/`string`/`file`/`algorithm` modules, `claude` 2.1.317 (`~/.local/bin/claude`), git, bash (the fake).

**Spec:** `/docs/superpowers/specs/2026-11-16-writing-tool-core-design.md`

## Global Constraints

- PgEngine branch `Chronicle`; commit only this plan's files (parallel sessions share the tree; no `git stash`). The tool writes into the site repo (`/home/corse/Documents/AstroTest`) only when a command is run; tests use temporary site folders, never the real one.
- `claude -p` never with `--bare`. Every call: `--output-format json --no-session-persistence --model <m> --system-prompt <s> --json-schema <schema> --tools <list>` and, when the list is not empty, `--allowedTools <the same list>` and `--add-dir <dir>` per folder to read. Prompt on stdin, `cwd` the engine.
- Claude never writes: tools are `""` or `"Read,Grep,Glob"` only.
- PgScript traps: int literals are 32-bit (build big numbers from smaller ones); about 120 `fun`s and 256 constants a file, functions before any big table; a second `fun` of the same name silently replaces the first (check with `grep -o "^fun [A-Za-z0-9_]*" tools/writer/lib/*.pg | sort | uniq -d`); a one-line `fun` does not parse; put a call's result in a `var` before reading its fields; `"s" + 5` is an error (use `toString`); `getArg(2)` is the first user argument.
- Files and shapes exactly as the spec: `config.json`, `guide.md`, `voices.md`, `themes.json`, `questions.md`, `ideas.json`, `articles/<slug>/{plan.md, chapters/NN-<voice>.md, chapters/NN.md, code/NN.md, record.json}`, `last-call.json`; the `plan.md` format; the record and ideas shapes; slug ≤ 60; tags `^[a-z0-9-]{1,40}$`.
- Each step's JSON schema carries `"title": "<step>"` (`ideas`, `plan`, `draft`, `code`, `stitch`, `share`, `theme`) — the fake `claude` picks its reply by it.
- Every command exits 0 on success, 1 on any error (`os.exit(1)`) after printing the reason.

## Review Focus

1. A `plan.md` edited by the author after drafting (a chapter added or removed, blank lines, a key left empty): the record's chapters follow the plan at each command, and `draft` of the new chapter works (Task 5).
2. Claude's answer missing a required field or empty: an error naming the step, nothing written (Task 2, through every step that uses `askClaude`).
3. Claude's text with `"`, `:`, newlines or `#` in a description, title or tag: the article header stays valid (every header value written as a double-quoted, escaped string) (Task 6).
4. `article ideas` with nothing new since the last scan: says so, makes no Claude call (Task 3).
5. A site folder without `writer/` (first run): defaults are written and listed; on later runs an existing default file the author changed is never overwritten (Task 1).

---

### Task 1: Foundations — util, config, the test runner, the fake `claude`

**Files:**
- Create: `/tools/writer/article.pg` (entry: `import "lib/..."` the libs, then `main()`), `/tools/writer/lib/util.pg`, `/tools/writer/lib/config.pg`
- Create: `/tools/writer/defaults/config.json`, `guide.md`, `voices.md`, `themes.json`, `questions.md` (the spec's first contents; `config.json`'s `site` is filled in when copied)
- Create: `/tools/writer/test/run.pg`, `/tools/writer/test/fake-claude.sh` (chmod +x), `/tools/writer/test/replies/` (one JSON file per step, Tasks 2–7 add theirs)

**Interfaces — Produces:**
- `util.pg`: `slugOf(title) -> string` (lowercase ASCII; runs of anything not `[a-z0-9]` → `-`; trimmed of `-`; cut to 60 then trimmed again; `"untitled"` when empty); `nn(n) -> string` (`"02"`); `fileText(path) -> string|none`; `readJson(path) -> value|none` (none when the file is missing); `writeJson(path, value)` (indent 2, parent folders made); `writeText(path, text)` (parent folders made); `nowMs() -> int` (`os.time()`); `today() -> string` (`"%Y-%m-%d"`); `fail(message)` (prints `article: <message>`, `os.exit(1)`); `has(list, item) -> bool`.
- `config.pg`: `siteDir() -> string` (`os.env("ARTICLE_SITE")` else `/home/corse/Documents/AstroTest`); `ensureWriter(site) -> list` (copies each missing default into `<site>/writer/`, returns the names written, never overwrites); `loadConfig() -> table` (runs `ensureWriter`, prints the names it wrote, reads `writer/config.json`, adds `writer` = `<site>/writer` and `site`).
- `article.pg`: `main()` reads `getArg(2)` (command) and the rest, dispatches; unknown or missing command → usage, exit 1. This task's commands: none yet besides usage.
- Test runner `run.pg`: `check(name, condition)` / `checkEq(name, got, want)` count and print failures; `runTool(site, args, env) -> {code, out, err}` runs `os.call("<env> /home/corse/Documents/PgEngine/release/PgCompilerBootstrap /home/corse/Documents/PgEngine/tools/writer/article.pg <args>")` (args shell-quoted by the runner); `tempSite() -> string` (`mktemp -d`, then a `writer/config.json` whose `claude` is the fake). Ends with `"<passed> passed, <failed> failed"`, `os.exit(1)` when any failed. `run.pg <group>` runs one group; no argument runs all.
- `fake-claude.sh`: reads its arguments; finds the `--json-schema` value's `"title"`; appends `argv` and stdin to `$FAKE_CLAUDE_LOG` when set; when `$FAKE_CLAUDE_FAIL` is set prints `{"type":"result","subtype":"error_during_execution","is_error":true,"result":"fake failure"}` and exits 1; else prints `{"type":"result","subtype":"success","is_error":false,"result":"","structured_output":<replies/<title>.json>}` (or `<replies/<title>-$FAKE_CLAUDE_VARIANT>.json` when that is set).

- [ ] **Step 1: Write the failing tests** (group `foundations` in `run.pg`)

```
checkEq("slug plain", slugOf("Closing the gap with Lua"), "closing-the-gap-with-lua")
checkEq("slug symbols", slugOf("  C++ & WASM: part 2!  "), "c-wasm-part-2")
checkEq("slug long", strlen(slugOf(<a title of 80 'a' then ' b'>)), 60)
checkEq("slug empty", slugOf("!!!"), "untitled")
checkEq("nn", nn(2), "02"); checkEq("nn 12", nn(12), "12")
// first run: a temp site with no writer/ -> `article` (no command) exits 1 with usage, and
// writer/ now holds config.json guide.md voices.md themes.json questions.md
// second run after editing guide.md -> guide.md unchanged, nothing listed as written   (Review Focus 5)
```

- [ ] **Step 2: Run** `cd /home/corse/Documents/PgEngine && release/PgCompilerBootstrap tools/writer/test/run.pg foundations` — Expected: fails (files missing).
- [ ] **Step 3: Implement** `util.pg`, `config.pg`, the defaults, `article.pg`'s `main` and usage, `fake-claude.sh`.
- [ ] **Step 4: Run** the same — Expected: `N passed, 0 failed`, exit 0.
- [ ] **Step 5: Commit** `git commit -m "Writing tool: foundations, config defaults, test runner"`.

### Task 2: The Claude call and the plan text

**Files:** Create `/tools/writer/lib/claude.pg`, `/tools/writer/lib/plan.pg`, `/tools/writer/prompts/` (empty for now); Modify `run.pg`; Create `/tools/writer/test/replies/theme.json`.

**Interfaces:**
- Consumes: `util.pg`, `config.pg`.
- Produces:
  - `fill(template, values) -> string` — each `{{name}}` replaced by `values[name]` (a missing name → `fail`).
  - `prompt(config, step) -> string` — `tools/writer/prompts/<step>.md` read relative to the tool folder.
  - `askClaude(config, step, promptText, schema, tools, dirs) -> table` — the schema (a table; `title` set to `step`) stringified; system prompt `"You are the <step> step of a writing tool. Answer only in the given JSON shape.\n\n"` + `guide.md`; model `config.models[step]` (`theme` uses `models.plan`); `os.call` as the Global Constraints say. Writes `<writer>/last-call.json` `{step, prompt, args, code, out, err, time}` every time. Returns the reply's `structured_output`. Errors → `fail("<step>: <reason>")`: non-zero code (with `err`), JSON that does not parse, `is_error` true, `subtype` not `"success"`, `structured_output` missing, or a `required` key of the schema absent from it.
  - `parsePlan(text) -> table` `{ok, plan, error}` where `plan` = `{title, theme, reader, knows, learns, question, chapters: [{n, title, answers, core, code, visual}]}` (rules of the spec; `core` kept as the raw comma text); `error` names the line.
  - `planText(plan) -> string` — the spec's format; `parsePlan(planText(p)).plan` equals `p`.

- [ ] **Step 1: Write the failing tests** (group `core`)

```
// round trip: a plan of 2 chapters -> planText -> parsePlan -> same title, theme, question, chapter 2's core and code
// author edits: extra blank lines, a key with nothing after ':', an unknown line -> still reads; the empty key is ""
// errors: no "# " title -> ok false, error mentions "title"; no "## " chapter -> ok false, error mentions "chapter"
// askClaude through the fake (a temp site; a test-only command `article _ask theme "text"` in article.pg that calls
// askClaude with a theme schema {theme: string, required [theme]} and prints the answer):
//   reply theme.json {"theme":"vm-performance"} -> prints vm-performance, exit 0; last-call.json has step "theme"
//   FAKE_CLAUDE_FAIL=1 -> exit 1, out contains "theme:"
//   FAKE_CLAUDE_VARIANT=empty (replies/theme-empty.json = {}) -> exit 1, out mentions "theme" (the missing key)   (Review Focus 2)
//   FAKE_CLAUDE_LOG: the logged argv has --output-format json, --tools, no --bare; stdin is the prompt
```

- [ ] **Step 2: Run** `release/PgCompilerBootstrap tools/writer/test/run.pg core` — Expected: fails.
- [ ] **Step 3: Implement** `claude.pg`, `plan.pg`, the `_ask` test command (listed in usage as internal).
- [ ] **Step 4: Run** the same — Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "Writing tool: the Claude call and the plan text"`.

### Task 3: Ideas

**Files:** Create `/tools/writer/lib/ideas.pg`, `/tools/writer/prompts/ideas.md`, `/tools/writer/test/replies/ideas.json`; Modify `article.pg`, `run.pg`.

**Interfaces:**
- Consumes: `askClaude`, `readJson`/`writeJson`, `loadConfig`.
- Produces: `loadIdeas(config) -> table` (`{lastScan: {commit: none, time: 0}, ideas: []}` when missing), `saveIdeas(config, ideas)`, `findIdea(ideas, id) -> table|none`, `addIdea(ideas, fields) -> table` (next `id`, `status "new"`, `created`, `slug none`); commands `ideas`, `ideas list`, `idea "<title>"`, `decline <id>`.
- `gatherDigest(config, sinceMs) -> string` — lines, each a list item, from: `git -C <engine> log --since=@<seconds> --format="%h %ad %s" --date=short --name-only` (time on the current branch); the paths of `CHANGELOG.md`, `examples/Chronicle/CHANGELOG.md` when changed since; files under `docs/superpowers/`, `benchmark/`, `test/pgcompiler/examples/adventofcode2025/` named in that log; `find <notes> -type f -newermt @<seconds>`; the text of `questions.md` (the author's own lines). Capped at 1500 lines (the rest dropped with a line saying how many). Empty when nothing changed.
- `ideas`: since = `lastScan.time`, or `nowMs()` − 60 days on the first run. Empty digest → prints "nothing new since <date>", no Claude call, `lastScan` unchanged. Else `askClaude("ideas", …, tools "Read,Grep,Glob", dirs [engine, notes])` with the digest and the existing ideas' titles and statuses (never proposed again); schema `{ideas: [{title, question, why, sources: [string], theme}]}`, `theme` one of `themes.json`'s keys; adds them, sets `lastScan` `{commit: <engine HEAD>, time: now}`, prints each as `<id>. <title> — <question>`.

- [ ] **Step 1: Write the failing tests** (group `ideas`; a temp engine repo made with `git init` and two commits, `config.engine` pointing at it, `notes` at a temp folder)

```
// ideas -> 2 ideas from replies/ideas.json added with ids 1, 2, status new; lastScan.time set; output lists "1." and "2."
// ideas again at once (no new commit, no changed note) -> "nothing new", FAKE_CLAUDE_LOG shows no second call   (Review Focus 4)
// the prompt sent (from the log) holds the commit subjects and the titles of existing ideas
// idea "My own" -> id 3, question ""; decline 2 -> status declined; ideas list shows new/declined groups
// decline 99 -> exit 1
```

- [ ] **Step 2: Run** `release/PgCompilerBootstrap tools/writer/test/run.pg ideas` — Expected: fails.
- [ ] **Step 3: Implement** `ideas.pg`, the prompt, the commands.
- [ ] **Step 4: Run** the same — Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "Writing tool: ideas"`.

### Task 4: Plan and the record

**Files:** Create `/tools/writer/lib/record.pg`, `/tools/writer/lib/steps_plan.pg`, `/tools/writer/prompts/plan.md`, `/tools/writer/test/replies/plan.json`; Modify `article.pg`, `run.pg`.

**Interfaces:**
- Consumes: `askClaude`, `planText`/`parsePlan`, `loadIdeas`/`saveIdeas`/`findIdea`, `slugOf`.
- Produces: `articleDir(config, slug)`, `loadRecord(config, slug) -> table|none`, `saveRecord(config, record)`, `newRecord(idea, slug, plan) -> table` (the spec's shape), `syncChapters(record, plan)` (the record's `chapters` follow the plan's: kept entries by `n` stay, new ones added with `drafts []`, `kept none`, removed ones dropped), `addStep(record, step, model)`, `loadPlan(config, slug) -> table` (fails with the line when unreadable); command `plan <id> [--again]`.
- `plan`: an idea `new` or `planned`; slug from the title (an existing article folder of another idea → slug + `-2`, `-3`…); `askClaude("plan", …, tools "Read,Grep,Glob", dirs [engine, notes])` with the idea, its sources, `themes.json`'s keys; schema `{title, theme, reader, knows, learns, question, chapters: [{title, answers, core, code, visual}]}`. Writes `plan.md` (numbering chapters 1..n) unless it exists: then prints that it exists and, with `--again`, writes `plan-new.md`; writes `record.json` when missing; idea → `planned`, `slug` set.

- [ ] **Step 1: Write the failing tests** (group `plan`)

```
// plan 1 -> articles/<slug>/plan.md parses, 3 chapters (replies/plan.json); record.json chapters n 1..3, steps [plan]; idea 1 planned with slug
// plan 1 again -> plan.md unchanged, message says it exists, exit 0; plan 1 --again -> plan-new.md written, plan.md unchanged
// plan 99 -> exit 1; plan of a declined idea -> exit 1
// a hand-edited plan.md with a 4th chapter -> loadPlan + syncChapters gives 4 chapters, chapters 1..3 kept as they were
```

- [ ] **Step 2: Run** `... run.pg plan` — Expected: fails.
- [ ] **Step 3: Implement** `record.pg`, `steps_plan.pg`, the prompt, the command.
- [ ] **Step 4: Run** — Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "Writing tool: plan and the record"`.

### Task 5: Code, draft, pick, mine

**Files:** Create `/tools/writer/lib/steps_draft.pg`, `/tools/writer/prompts/code.md`, `draft.md`, replies `code.json`, `draft.json`; Modify `article.pg`, `run.pg`.

**Interfaces:**
- Consumes: `loadPlan`, `loadRecord`/`saveRecord`/`syncChapters`/`addStep`, `askClaude`.
- Produces: `voicesOf(config) -> list` of `{name, text}` (each `## name` section of `voices.md`); commands `code <slug> <n>`, `draft <slug> <n> [--again]`, `pick <slug> <n> <voice> [--replace]`, `mine <slug> <n>`.
- `code`: `askClaude("code", …, tools "Read,Grep,Glob", dirs [engine])` with the chapter's `code:` line and the plan; schema `{snippets: [{file, lines, code, why}]}`; writes `code/NN.md` (per snippet: `` `file:lines` ``, the why line, a fenced block with the language guessed from the extension: `.pg` → `javascript` as the site's docs do, `.cpp/.h` → `cpp`, else none).
- `draft`: the record synced to the plan first; when the chapter's `code` is not empty and not `none` and `code/NN.md` is missing → runs `code` first. One `askClaude("draft", …, tools "")` per voice with guide, voice text, plan text, the kept chapters before `n` in order (each `NN.md` with its title), `code/NN.md`; schema `{text}`. Writes `chapters/NN-<voice>.md` (an existing one only with `--again`; without, that voice is skipped and said so). Never writes `NN.md`. Records `drafts` (voice names) and `draftedAt`.
- `pick`: copies `NN-<voice>.md` to `NN.md`; when `NN.md` exists and equals no draft of that chapter → refuses (exit 1) unless `--replace`. Records `kept`, `keptAt`. `mine`: `NN.md` must exist; records `kept "mine"`.

- [ ] **Step 1: Write the failing tests** (group `draft`; an article planned as in Task 4)

```
// draft <slug> 2 with chapter 2's code "the table int keys" and no code/02.md -> code/02.md written first (log: a code call before the draft calls),
//   then chapters/02-guide.md, 02-story.md, 02-brief.md; no 02.md; record chapter 2 drafts [guide, story, brief]
// the draft prompt (log) for chapter 2 holds chapter 1's kept text when 01.md exists, and code/02.md's text
// draft again without --again -> files unchanged, says skipped; with --again -> rewritten
// pick <slug> 2 story -> 02.md == 02-story.md, kept story; edit 02.md by hand, pick <slug> 2 guide -> exit 1, 02.md unchanged;
//   with --replace -> 02.md == 02-guide.md
// mine <slug> 3 without 03.md -> exit 1; with 03.md -> kept mine
// plan.md given a 4th chapter after drafting -> draft <slug> 4 works, status/record has 4 chapters   (Review Focus 1)
// pick of a voice not in voices.md -> exit 1
```

- [ ] **Step 2: Run** `... run.pg draft` — Expected: fails.
- [ ] **Step 3: Implement** `steps_draft.pg`, the prompts, the commands.
- [ ] **Step 4: Run** — Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "Writing tool: code, draft, pick, mine"`.

### Task 6: Stitch

**Files:** Create `/tools/writer/lib/steps_publish.pg`, `/tools/writer/prompts/stitch.md`, reply `stitch.json`; Modify `article.pg`, `run.pg`.

**Interfaces:**
- Consumes: `loadPlan`, record functions, `askClaude`.
- Produces: `editedShare(kept, draft) -> double` (the share of `kept`'s non-empty lines not found among `draft`'s lines, trimmed; 0.0 when `kept` has none); `yamlString(text) -> string` (a double-quoted string, `\` `"` and control characters escaped, as JSON writes them); command `stitch <slug>`.
- `stitch`: every chapter kept (else exit 1 naming the chapters left). `askClaude("stitch", …, tools "")` with the plan and the kept chapters; schema `{description, excerpt, tags: [string], findings: [string]}`. Writes `<site>/src/content/blog/<slug>.md`: `---`, `title`, `description`, `pubDate` (the record's first publish date, today the first time), `updatedDate` (today, from the second stitch on), `tags` (a YAML list of `yamlString`s), `author` (config), `excerpt`, `---`, a blank line, then each chapter `## <title>` + blank line + its `NN.md` text unchanged. No `# ` line (the layout shows the title). Every value `yamlString`. Each chapter's `edited` (`null` for `mine`), `published {path, time}`, `checks` += `{time, findings}`, idea → `written`. Prints the path and the findings.

- [ ] **Step 1: Write the failing tests** (group `stitch`; an article with 3 chapters kept: 01 a pick untouched, 02 a pick edited on 1 of its 4 lines, 03 mine)

```
// stitch -> the blog file starts with "---", has exactly 3 lines starting "## ", in order, each followed by its NN.md text unchanged
// header: title/description/pubDate/author/excerpt/tags present; pubDate today; no updatedDate
// reply stitch.json with a description holding "quote", a colon, a newline and # -> the header line is yamlString of it
//   (parse the header back by stripping the --- block and checking the description line equals the escaped form)   (Review Focus 3)
// record: edited 0.0 for chapter 1, 0.25 for chapter 2, none for 3; checks has the findings; idea written
// stitch again -> pubDate unchanged, updatedDate today
// stitch with chapter 2 not kept -> exit 1, message names 2
// editedShare unit: ("a\nb\nc", "a\nb\nc") 0.0; ("a\nX\nc\nd", "a\nb\nc\nd") 0.25; ("", "x") 0.0
```

- [ ] **Step 2: Run** `... run.pg stitch` — Expected: fails.
- [ ] **Step 3: Implement** `steps_publish.pg` (stitch part), the prompt, the command.
- [ ] **Step 4: Run** — Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "Writing tool: stitch"`.

### Task 7: Share and status

**Files:** Modify `/tools/writer/lib/steps_publish.pg`; Create `/tools/writer/prompts/share.md`, reply `share.json`; Modify `article.pg`, `run.pg`; Create `/tools/writer/README.md`.

**Interfaces:**
- Produces: `tagFor(record, place, k) -> string` — `place` is `r-<sub>` for a Reddit community (lowercase, `r/` dropped, other characters → `-`) or `x`; then `-<short>-<k>` where `short` is the slug cut so the whole tag is at most 40 characters (trimmed of `-`); `k` the next number making it unique among the record's tags; commands `share <slug>`, `status <slug>`.
- `share`: the article stitched (else exit 1). For each place of the theme in `themes.json`: `askClaude("share", …, tools "")` with the article's title, description, excerpt, plan question, the place, and the link `https://columbaengine.org/blog/<slug>/?from=<tag>`; schema `{title, text}` (`title` empty for X). An X text over 280 characters → `fail`. Appends `{platform, where, tag, title, text, time}` to `posts` (never replaces), prints each ready to paste with a separator line.
- `status`: title, theme, each chapter (`n`, title, drafts, kept, edited), the last check's findings, the posts (place, tag, time).
- `README.md`: setup (`alias article='/home/corse/Documents/PgEngine/release/PgCompilerBootstrap /home/corse/Documents/PgEngine/tools/writer/article.pg'`), the commands in order, the files, how to run the tests.

- [ ] **Step 1: Write the failing tests** (group `share`)

```
// tagFor: r/cpp -> "r-cpp-<short>-1"; x -> "x-<short>-1"; a second r/cpp post -> "-2"; every tag matches ^[a-z0-9-]{1,40}$ for a 60-char slug
// share (theme vm-performance: 3 places) -> 3 posts recorded, tags unique, each text from the reply, link contains ?from=<its tag>
// share again -> 6 posts, the first 3 unchanged
// an X reply of 300 characters (FAKE_CLAUDE_VARIANT=long) -> exit 1, posts unchanged
// share before stitch -> exit 1
// status -> lists the 3 chapters with kept voices and the posts' tags
```

- [ ] **Step 2: Run** `... run.pg share` — Expected: fails.
- [ ] **Step 3: Implement** share, status, `tagFor`, the README.
- [ ] **Step 4: Run** `release/PgCompilerBootstrap tools/writer/test/run.pg` (all groups) — Expected: `N passed, 0 failed`; and `grep -o "^fun [A-Za-z0-9_]*" tools/writer/lib/*.pg tools/writer/article.pg | sed 's/.*://' | sort | uniq -d` prints nothing.
- [ ] **Step 5: Commit** `git commit -m "Writing tool: share, status, README"`.

### Task 8: A real run on the subscription (with the author)

- [ ] **Step 1:** With the author's go-ahead (it spends their plan's usage), in a copy of the site (`ARTICLE_SITE=<scratch copy of AstroTest>`): `article ideas`, then `plan` on the idea the author picks, `draft` of chapter 1, `pick`, and report what came back (the ideas, the plan, a draft) for the author to read. Fix what the real `claude` does differently from the fake (record a `Ruling:` for each), with a test for each fix.
- [ ] **Step 2: Commit** any fixes.
