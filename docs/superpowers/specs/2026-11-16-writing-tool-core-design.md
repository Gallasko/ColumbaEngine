# The writing tool, core (project 3)

2026-11-16. Project 3 of six towards regular articles on columbaengine.org. Project 1 (site
analytics) is live; project 2 (`import ... as`, `json`, `os` in PgScript) is built on `Chronicle`.

## Why

The author wants to publish regularly to grow an audience for ColumbaEngine and its games. What
stops them: finding what to write about, finding the engine code to show, and an article's thread
(*fil rouge*) that does not teach well enough. They also want Reddit/X posts prepared from each
article, and every article and post recorded, so the site's analytics can be read against how each
was written. The tool is written in PgScript (the author's choice, and a showcase for the
language) and drafts with Claude through the author's subscription.

## Goals

- `article ideas`: ideas found in the author's recent work, each with the question it answers and
  why it is worth reading; the author's own ideas added by hand.
- `article plan`: an idea made into a teaching plan, the author edits it before anything is drafted.
- `article draft` / `pick`: each chapter drafted in several voices; the author keeps one or writes
  their own.
- `article code`: the engine code a chapter needs, found and trimmed.
- `article stitch`: the chapters joined into a site article, with checks of the teaching thread.
- The order of the steps: plan → (code) → draft → pick → stitch → share.
- `article share`: Reddit and X posts by the article's theme, with `?from=` tags, printed to paste.
- Everything recorded per article.

## Non-goals

- No posting to Reddit or X: posts are prepared and printed; the author posts by hand.
- No visuals (project 4): a chapter may hold a marker `<!-- visual: ... -->`, kept as is.
- No web page (project 5), no live code boxes (project 6), no split testing, no short videos.
- No `git commit` or deploy of the site: the tool writes files; the author commits and deploys.
- No reading of Discord or Reddit: questions come from a file the author fills.

## Anti-goals

- Claude writing, editing or running anything: it reads and answers; the tool writes every file.
- A draft overwriting text the author wrote or kept.
- Records in a drifting format: one shape, written only by the tool.
- Teaching that pads: general programming (loops, functions, hash maps, pointers) explained to a
  reader who knows a language, unless it is the article's very subject.
- An API key or a per-call bill: the subscription only.

## Constraints

- **Claude through the subscription:** `claude -p` (Claude Code 2.1.317 at `~/.local/bin/claude`)
  logs in with the author's subscription. Never `--bare`: that mode reads only an API key. Calls
  count against the plan's usage limits.
- **The call that works** (checked 2026-11-16): prompt on stdin, and
  `claude -p --output-format json --no-session-persistence --model <m> --system-prompt <s> --json-schema <schema> --tools <t>`;
  the reply is JSON with `is_error` (false), `subtype` (`"success"`), `result`, and
  `structured_output` (the object matching the schema). `--tools ""` gives no tools; reading needs
  `--tools "Read,Grep,Glob"` plus `--add-dir <folder>` for each folder outside the working one.
- **PgScript as it is** (`/docs/compiler/QUICK_START.md`, and the facts below): `import "os" as os`
  and `import "json" as json` (project 2), run by `release/PgCompilerBootstrap`. Int literals are
  32-bit. A file holds about 120 functions (split the tool into several files). Reading a field
  straight off a table a call has just built can read freed memory: put results in a `var` first.
  A one-line `fun` or `class` does not parse. `getArg(2)` is the first user argument.
- **The site:** `/home/corse/Documents/AstroTest`. Articles are `src/content/blog/<slug>.md`; their
  header (the content schema in `src/content/config.ts`): `title`, `description`, `pubDate`, and
  optional `updatedDate`, `heroImage`, `tags`, `author` (`"PigeonCodeur"`), `excerpt`. A chapter is
  an h2: the site's analytics measure chapters reached by h2, so a chapter must be one `## ` heading.

## Approaches considered

- **A PgScript tool driving `claude -p` read-only** (chosen): the tool builds each prompt from the
  author's guide, voices, the plan and the chapters so far; Claude answers in a fixed JSON shape
  with only Read/Grep/Glob; the tool writes every file, so the records keep one format.
- Claude in agent mode writing the files itself: less code, but records drift and a bad run can
  touch files outside `writer/`.
- Skills inside a Claude Code session (`/article-plan`): no tool, but not PgScript, which the author
  asked for and wants as a showcase.

## Design

### Files

The tool's code in PgEngine (`/tools/writer/`); the author's material and records in the site repo
(`/home/corse/Documents/AstroTest/writer/`), so an article and its history live together.

```
PgEngine/tools/writer/
  article.pg        the entry point: reads the command, dispatches
  *.pg              the rest, split by concern (claude call, records, plan text, steps)
  prompts/          one prompt template per step (ideas, plan, draft, code, stitch, share), with
                    {{placeholders}}; part of the tool, versioned with it
  test/             tests (below)

AstroTest/writer/
  config.json       paths, models, author (below)
  guide.md          the teaching rules: the reader, core vs general, the chapter shape; given to
                    every step; the author edits it to tune the tool
  voices.md         the voices, one "## name" section each with how it writes
  themes.json       theme → where its posts go
  questions.md      questions pasted from Discord or Reddit
  ideas.json        the ideas and the last scan
  articles/<slug>/
    plan.md         the plan, plain text the author edits
    chapters/NN-<voice>.md   drafts;  NN.md   the chapter kept (or written by the author)
    code/NN.md      the code for chapter NN
    record.json     the article's record
```

`NN` is the chapter number on two digits (`02`). A `slug` is the title lowercased, runs of other
characters turned to `-`, trimmed, at most 60 characters; it never changes once made.

### `config.json`

```json
{
  "claude": "/home/corse/.local/bin/claude",
  "engine": "/home/corse/Documents/PgEngine",
  "notes": "/home/corse/.claude/projects/-home-corse-Documents-PgEngine/memory",
  "site": "/home/corse/Documents/AstroTest",
  "author": "PigeonCodeur",
  "models": { "ideas": "opus", "plan": "opus", "draft": "sonnet", "code": "sonnet", "stitch": "opus", "share": "sonnet" }
}
```

The tool finds it as `writer/config.json` under the site folder given by the `ARTICLE_SITE`
environment variable, else `/home/corse/Documents/AstroTest`.

### The guide (first content of `guide.md`, written by the tool's first run when missing)

- The default reader knows a programming language well and does not know ColumbaEngine. An
  article's plan may name another reader.
- The article's **core** is explained in full, every step: what it is, why it matters, how it
  works here, what changed. Anything that exists only in ColumbaEngine or PgScript is core.
- **General programming** (loops, functions, hash maps, pointers, recursion) is never explained,
  unless it is the article's subject (a loop optimization article explains how the VM runs a loop).
- One question is carried through the article. Each chapter is one step of the answer, in an order
  where nothing is used before it is explained. A chapter: why it matters, a concrete example, the
  explanation, what to keep.
- Each chapter opens by saying how it answers what the previous one left open.

### The voices (first content of `voices.md`)

`guide` (step by step, "you", plain and direct), `story` (first person: how it was found, what
failed first), `brief` (numbers and code first, short). The author may edit, add or remove voices;
a draft is made in every voice listed.

### `themes.json` (first content)

```json
{
  "vm-performance": [{"platform": "reddit", "where": "r/ProgrammingLanguages"}, {"platform": "reddit", "where": "r/cpp"}, {"platform": "x"}],
  "engine":         [{"platform": "reddit", "where": "r/gamedev"}, {"platform": "x"}],
  "chronicle":      [{"platform": "reddit", "where": "r/incremental_games"}, {"platform": "x"}],
  "web":            [{"platform": "reddit", "where": "r/webdev"}, {"platform": "x"}]
}
```

### `plan.md`

```
# <title>
theme: vm-performance
reader: a developer who knows a language well, not ColumbaEngine
knows: <what the reader already knows>
learns: <what they know at the end>
question: <the one question carried through>

## 1. <chapter title>
answers: <what this chapter answers, and how it follows from the previous one>
core: <core topics this chapter explains, comma separated>
code: <the code it needs, in words, or "none">
visual: <a visual it wants, or "none">

## 2. ...
```

The tool writes it from Claude's structured answer and reads it back by these rules: the first
`# ` line is the title; `key: value` lines before the first `## ` are the article's; each
`## N. title` starts a chapter, the `key: value` lines under it are that chapter's; other lines are
ignored. A missing key is empty. A plan the tool cannot read (no title, no chapter) is an error
naming the line.

### `ideas.json`

```json
{
  "lastScan": {"commit": "<PgEngine commit>", "time": 1794830998386},
  "ideas": [{"id": 1, "title": "...", "question": "...", "why": "...", "sources": ["..."],
             "theme": "vm-performance", "status": "new", "created": 1794830998386, "slug": null}]
}
```

`status` is `new`, `planned` (a plan exists, `slug` set), `written` (stitched), or `declined`.
`id` grows by one and never comes back.

### `record.json`

```json
{
  "slug": "...", "title": "...", "idea": 3, "theme": "vm-performance",
  "created": 1794830998386, "reader": {"reader": "...", "knows": "...", "learns": "...", "question": "..."},
  "chapters": [{"n": 1, "title": "...", "drafts": ["guide", "story", "brief"], "kept": "story", "edited": 0.12, "draftedAt": 0, "keptAt": 0}],
  "steps": [{"step": "plan", "model": "opus", "time": 0}],
  "published": {"path": "src/content/blog/<slug>.md", "time": 0},
  "checks": [{"time": 0, "findings": ["..."]}],
  "posts": [{"platform": "reddit", "where": "r/cpp", "tag": "r-cpp-<slug-short>-1", "title": "...", "text": "...", "time": 0}]
}
```

`kept` is a voice name, `"mine"` when the author wrote `NN.md` themselves, or `null`. `edited`
(set by `stitch`, my call) is the share of `NN.md`'s lines not found unchanged in the kept draft
(0 untouched, 1 rewritten; `null` for `mine`), so the voice an article is credited with can be read
against how much the author changed it.

### Commands

`release/PgCompilerBootstrap tools/writer/article.pg <command> ...`; the author may alias it to
`article`. Every command prints what it wrote. A command given wrong arguments prints its usage.

- **`article ideas`** — gathers what changed since `lastScan`, by time on the current branch (all of
  it on the first run, capped to the last 60 days): PgEngine's commits (subjects and the files they touched), the two
  `CHANGELOG.md`, the design and plan documents under `docs/superpowers/` changed since, the
  benchmark sources and results under `benchmark/`, the AoC folder
  `test/pgcompiler/examples/adventofcode2025/`, the notes folder's files changed since, and
  `questions.md`. It hands Claude this digest (a list, not the contents) with Read/Grep/Glob and
  `--add-dir` for the engine and the notes, the ideas already in `ideas.json` (so none is proposed
  twice, including declined ones), and asks for new ideas grouped by topic (never one per commit).
  Adds them as `new`, sets `lastScan`, prints them numbered by `id`.
- **`article ideas list`** — prints the ideas by status. **`article idea "<title>"`** adds one by hand
  (question and why empty, theme asked of Claude at plan time). **`article decline <id>`**.
- **`article plan <id>`** — the idea's slug made; Claude (tools Read/Grep/Glob, engine and notes)
  writes the plan following `guide.md`, as structured output; written to `plan.md` and a new
  `record.json`; the idea becomes `planned`. A `plan.md` that exists is never overwritten (the
  command says so; `--again` writes `plan-new.md` beside it for the author to compare).
- **`article draft <slug> <n>`** — chapter `n` drafted in each voice of `voices.md`, from `guide.md`,
  the voice, `plan.md`, the chapters kept before `n` (`NN.md` in order), `code/NN.md`. When the
  chapter's `code:` line is not `none` and `code/NN.md` is missing, the code step runs first (my
  call: stitch adds no code, so code only reaches the article through the draft). Each draft opens
  with its own join to the previous chapter, as the guide asks (it sees the chapters kept before it).
  One Claude call per voice. Written to `chapters/NN-<voice>.md`; an existing draft file is
  replaced only with `--again`. Never touches `NN.md`.
- **`article pick <slug> <n> <voice>`** — copies `NN-<voice>.md` to `NN.md` and records `kept`.
  Refuses when `NN.md` exists and differs from every draft (the author's own text) unless
  `--replace`. **`article mine <slug> <n>`** records `kept: "mine"` for a `NN.md` the author wrote.
- **`article code <slug> <n>`** — Claude (Read/Grep/Glob in the engine) finds the code the
  chapter's `code:` line asks for and returns snippets, each with its file, line range, the trimmed
  code and one line on why it is there; written to `code/NN.md`.
- **`article stitch <slug>`** — needs every chapter kept. Claude reads the plan and the kept
  chapters and returns: a `description` and `excerpt` for the header, `tags`, and findings: a
  chapter whose opening does not follow from the previous chapter, a core topic used before its
  chapter explains it, general programming explained at length, a step the reader cannot follow, a
  chapter that does not answer its `answers:` line. Stitch adds no text of its own (my call: the
  joins are the drafts', so the kept text reaches the site exactly as kept). The tool writes
  `src/content/blog/<slug>.md`: the header (`pubDate` today, `author` from config), then each
  chapter as `## <title>` and its kept text; it computes each chapter's `edited`. It prints the
  findings and stores them in `checks`; the idea becomes `written`. An existing article
  file is replaced (the author fixes chapters and stitches again); `updatedDate` is set then.
- **`article share <slug>`** — needs the article stitched. For each place of its theme in
  `themes.json`, Claude writes a post (a Reddit post: title and text; an X post: at most 280
  characters with the link) fitted to that community, linking to
  `https://columbaengine.org/blog/<slug>/?from=<tag>`; the tag is `<platform-or-sub>-<short>-<k>`
  (lowercase letters, digits, dashes, at most 40, as the site's analytics accept), unique in the
  record. Stored in `posts`, printed ready to paste. Running it again adds new posts, never
  replaces the old ones (their tags may already be out).
- **`article status <slug>`** — the record in short: chapters, drafts, kept, checks, posts.

### Claude calls

One function makes every call: the step's prompt template filled in, the system prompt (the
step's role plus `guide.md`), the step's model from config, its JSON schema, its tools. It runs
`claude` through `os.call` with the prompt as `input`, the engine as `cwd`. A non-zero code,
unreadable JSON, `is_error` true or a `subtype` other than `"success"` is an error: the command
prints the reason (and `err`) and writes nothing. Each successful step is appended to the record's
`steps`. The prompt sent and the answer of the last call are kept in `writer/last-call.json` for
the author to see what went wrong.

### Tests

- A fake `claude`: a shell script in `tools/writer/test/` that reads its arguments and stdin and
  prints a fixed reply per step (chosen by the schema's name, a field every step's schema carries),
  and can be told to fail. The tests point `config.json`'s `claude` at it.
- A PgScript test script (`tools/writer/test/run.pg`, run with `release/PgCompilerBootstrap`) builds
  a temporary site folder, runs each command against the fake, and checks the files written and the
  records: the plan read back equals the plan written; `pick` refuses the author's own text;
  `draft` never touches `NN.md`; `stitch` gives one `## ` per chapter and a header the site's
  schema accepts; `share` tags are unique and valid; a failing Claude call writes nothing.
- Pure parts tested directly: slug, plan reading (including a plan with a missing key and one with
  no chapter), the tag rule.
- One real run by hand on the author's subscription at the end: `ideas`, then one article through
  `share`, read by the author.

## Left to the builder

- How the tool is split into files, and the prompt templates' wording (the guide's rules above are
  the substance; prompts must say "answer in the given shape").
- The JSON schemas of each step's answer.
- How big the ideas digest may get before it is cut (it lists, it does not paste contents).
- The usage text of each command.

## Where this fits

(1) site analytics, live; (2) PgScript `import ... as`, `json`, `os`, built; (3) this; (4) SVG
visuals in PgScript, filling the `visual:` markers; (5) a local web page served by PgScript over
these commands; (6) live PgScript code boxes in articles.
