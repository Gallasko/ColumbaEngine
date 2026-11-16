# The writing app (project 5)

2026-11-16. Project 5 of six towards regular articles on columbaengine.org. Projects 1–3 are built:
the site's analytics (live), PgScript's `import ... as` / `json` / `os` (on `Chronicle`), and the
`article` tool (`/tools/writer/`, used for real by the author on 2026-11-16).

## Why

The `article` tool works but lives in a terminal, and the author writes the chapters in another
editor. The author wants a visual tool: an app built with ColumbaEngine's own UI, holding a real
text editor and the pipeline of steps that takes an idea to a published article, with a preview
in the browser showing the real site.

## The author's decisions

- An app built with the engine's UI (not a browser page, not a VS Code panel).
- An editor with Markdown help: colouring and search (option b), on top of solid plain editing.
- The preview is the real site, run locally (`astro dev`), never in production.
- Layout B: the pipeline across the top, the files on the left, the editor in the middle, the
  commands' log at the bottom; an Ideas screen; the step waiting for the author holds the main
  button.

## Goals

- **A multi-line text editor widget in the engine** (`src/Engine/UI/`), usable by any app.
- **The Writer app** (`examples/Writer/`, target `Writer`): the window of layout B over the `article`
  tool.
- **A preview page in the site** that exists only under `astro dev`.
- **The `article` tool tells its state as JSON**, so the app shows the pipeline without its own
  copy of the rules.

## Non-goals

- Syntax colouring inside code blocks, spell-check, several cursors, find-and-replace (later).
- The mind map and the charts (project 4: a third screen and the visual markers, later).
- Running on Windows or the web (the `article` tool's `os.call` is Linux/macOS; the app is a
  desktop tool for the author's machine).
- Any writing logic in the app: every step is an `article` command.

## Anti-goals

- The window freezing while a Claude call runs (a minute or more).
- Losing the author's text: an unsaved edit overwritten by a command, or by a reload.
- A preview page reachable in production, or a preview visit measured by the analytics.
- Two commands on the same article at once.

## Constraints

- **Engine UI as it is:** `TextInputComponent` is single-line (`acceptMultilines` is a to-do).
  `TTFText` draws text with markup (`\n`, and `\c{}` colour codes) and
  `TTFTextSystem::measureText(font, text, scale, maxWidth, ...)` measures without drawing (the
  same layout as the drawing). `ScrollableComponent`, `ListView`, buttons, tooltips, themes and
  `FocusableComponent` exist. Input arrives as `OnSDLTextInput`, `OnSDLScanCode`, mouse events.
  `examples/BasicTerminal` draws many lines with a prefab a line (prior art). Nothing uses the
  clipboard yet; SDL2's `SDL_GetClipboardText` / `SDL_SetClipboardText` are there.
- **The `article` tool:** `release/PgCompilerBootstrap tools/writer/article.pg <command>`, the site
  in `$ARTICLE_SITE` or `/home/corse/Documents/AstroTest`; its files as in
  `/docs/superpowers/specs/2026-11-16-writing-tool-core-design.md`.
- **Running a process from C++:** the posix_spawn + pipes + poll code of `os.call`
  (`src/Engine/Helpers/osmodule.cpp`) is compiled only into the command-line tools and
  `test_compiler`, never into an engine library a game links. The app may share it the same way.
- **The site:** Astro 5.12 with `@astrojs/cloudflare`, `output: 'static'`, deployed from a local
  build; its pages use `Layout.astro`, which includes the analytics script (`Measure.astro`).
- **Builds:** in `/release`, `make -j4 <target>` when the author allows building in the session.

## Approaches considered

- **A C++ app on the engine, the editor as a new engine widget, every step an `article` command
  run in the background** (chosen, from the author's choices).
- The app's UI written in PgScript: the UI module gives scripts four text functions; the whole UI
  would have to be bound to PgScript first.
- An outside editor (open the file in VS Code): the author chose a real editor in the app.

## Design

Built in four plans, in this order; each one works and is tested on its own.

### Part 1 — the `article` tool says its state (PgScript)

- **`article state <slug>`** prints one JSON object:
  `{slug, title, theme, chapters: [{n, title, code, files: {kept, drafts: [voice], code}, kept, edited, markers}], published: {path, date}|null, posts: n, next: {step, n|null, label, args: [..], needsAuthor}}`.
  `markers` counts the `<!-- visual: ... -->` and `<!-- number: ... -->` left in the chapter's kept
  file (or, when not kept, in its drafts). `next` is the first of: `plan` missing; for each chapter
  in order, `draft` when it has no draft, `pick` (needs the author) when drafted but not kept;
  `mine` when the chapter's kept file exists, holds text that is no draft's, and the record has no
  `kept` (args `["mine", slug, n]`, not needing the author: the text is already theirs); `stitch`
  when every chapter is kept and the article was never stitched or a kept file changed since
  (compared by content: the builder keeps what it needs at stitch); `share` when stitched and no
  post yet; else `done`. For `pick`, `args` is `["pick", slug, n]`: the app adds the voice. `args` is the command to run
  (`["draft", slug, "2"]`), empty for steps the author does by hand.
- **`article ideas list --json`** prints the ideas as `ideas.json` holds them.
- **`article articles --json`** prints `[{slug, title, next}]` for every article folder.
- Tests in the tool's runner, as the others.

### Part 2 — the text editor widget (engine)

Two layers, so the logic is tested without a window.

- **The model (plain C++, no ECS, no rendering)**, in `src/Engine/UI/texteditor/`:
  - `TextBuffer`: lines, a cursor `{line, column}` (byte columns, UTF-8 aware: never inside a
    code point), a selection anchor; insert text, new line, backspace/delete (a character, a
    word with Ctrl), moves (character, word, line start/end, document start/end, a page of
    `visibleLines`), select by any move with Shift, select all; get the selected text.
  - Undo/redo: edits grouped (typing within a word, until a pause of 1 s, a move or a different
    kind of edit), each group undone at once.
  - `wrapLines(buffer, width, measure)`: the visual lines of each buffer line wrapped at `width`
    by a measure function (the real `measureText` in the widget, a fixed-width one in tests),
    and mapping both ways between `{line, column}` and `{visual line, x}`.
  - `markdownSpans(lines)`: for each line, spans `{start, end, kind}` with kinds `heading`,
    `bold`, `italic`, `inlineCode`, `fence`, `codeBlock` (the lines between fences),
    `link`, `listMarker`, `writerMarker` (`<!-- visual: ... -->`, `<!-- number: ... -->`),
    `plain`; a fence opens and closes across lines.
  - `findAll(lines, query)` (case-insensitive) → matches `{line, start, end}`; next and previous
    from the cursor.
- **The component and system (`TextEditorComponent`, `TextEditorSystem`)**:
  - Renders only the visual lines in view, a `TTFText` a line with `\c{}` colour codes per span
    (colours from the theme), a gutter of line numbers, the cursor (blinking), the selection and
    the search matches as rectangles behind the text.
  - Input when focused: text input, the keys of the model (arrows, Home/End, PageUp/Down,
    Ctrl+arrows, Shift to select, Ctrl+A, Backspace/Delete, Enter, Tab inserts 4 spaces), Ctrl+C /
    Ctrl+X / Ctrl+V through the SDL clipboard, Ctrl+Z / Ctrl+Y (and Ctrl+Shift+Z), mouse click to
    place the cursor, drag to select, double click selects a word, the wheel to scroll; Ctrl+F
    opens a find field above the text (Enter / Shift+Enter: next / previous, Escape closes).
  - API: `setText(text)`, `text()`, `dirty()`, `markSaved()`; events `TextEditorChanged{id}` and
    `TextEditorSaveRequested{id}` (Ctrl+S).
  - Works with a few thousand lines: drawing only what is in view, wrapping redone only for lines
    that changed or when the width changes.
- **Tests:** a gtest target for the model (every move, edit, undo group, the UTF-8 rule, wrap
  mapping, each Markdown kind including fences across lines, search). The widget itself is
  checked by hand in the app (and in a small demo scene if the builder wants one).

### Part 3 — the preview in the site (AstroTest)

- An integration `src/integrations/writer-preview.mjs` in `astro.config.mjs`: in
  `astro:config:setup`, only when `command === 'dev'`, it injects the route `/preview/[slug]` from
  `src/preview/preview.astro` (a folder no build reads), server-rendered; in `astro:server:setup`
  it watches `writer/articles/` and sends the browser a full reload when a file there changes.
- **The blog page's look, shared (my call):** the article markup and styles now in the scoped
  `<style>` of `src/pages/blog/[...slug].astro` move into a component (`ArticleBody.astro`) that
  both the blog page and the preview use, so the preview cannot drift; the built blog pages are
  checked to come out the same (a `dist/` diff before and after). The preview leaves out Giscus.
- `preview.astro` reads the article's `plan.md`, `record.json` and chapter files from
  `writer/articles/<slug>/`, and renders what `article stitch` would publish: the title, the
  chapters as `## title` with their kept text; a chapter not kept shows the draft named by
  `?draft=NN-<voice>` when it is that chapter's, else a placeholder ("chapter 2: not written
  yet"). The writer markers are shown as visible notes. Markdown is rendered with Astro's own
  Markdown processor (the one the blog uses), inside `Layout.astro` and the blog's `prose.css`.
- `Layout.astro` takes a `measure` prop (default `true`); the preview passes `false`, so the
  analytics script is not on the page.
- A production build has no `/preview` route and no file-reading code (checked: `npm run build`
  then `dist/` has no `preview`).

### Part 4 — the Writer app (`examples/Writer/`, target `Writer`)

- **The window**, as layout B:
  - a top bar: the open article's title, an **Ideas** button (the Ideas screen), **Preview ↗**;
  - the pipeline strip: a chip a step from `article state` (plan, each chapter, stitch, share),
    green done, amber waiting for the author (outlined), blue running, grey to do; the main
    button on the right runs `next.args` (or says what to do by hand: "edit chapters/02.md");
    clicking a chip opens what it made (the plan, a chapter's kept file or drafts, the
    published file, the posts as a read-only page);
  - files on the left: `plan.md`, each chapter's drafts and kept file, `guide.md`, `voices.md`;
  - the editor in the middle; a status line under it (saved / unsaved, markers left, the
    cursor's line);
  - the log at the bottom, a few lines: what runs, what it printed; a failure in red with its
    reason.
- **Keeping a chapter (my call):** when `next` is `pick n`, the main button reads "Keep this draft"
  and runs `pick` with the voice of the draft open in the editor (disabled, "open a draft to keep
  it", when none is). Each chapter in the file list has "Write my own": it creates
  `chapters/NN.md` (empty) and opens it; once it holds text that is no draft's, `next` becomes
  `mine n` and the main button reads "Keep my text".
- **What is editable (my call):** `plan.md`, the kept chapters `NN.md`, `code/NN.md`, `guide.md`,
  `voices.md`, `questions.md`. Read-only: the drafts (keep one to edit it) and the published
  article (stitch rewrites it). Commands that replace text ask first: "Redraft chapter n"
  (`draft --again`), "Find the code again" (`code`, over an existing `code/NN.md`), "Keep this
  draft" over a kept file that is the author's own or edited (`pick --replace`). "A new plan
  beside this one" (`plan --again`) writes `plan-new.md`, nothing to confirm.
- **Articles (my call):** the title in the top bar is a menu of the articles (`article articles
  --json`); the Ideas screen links a planned or written idea to its article. **One command at a
  time in the whole app** (several steps rewrite `ideas.json`): while one runs, the other command
  buttons are disabled; editing, opening and switching stay available; the log is the app's,
  one for all.
- **The Ideas screen** replaces the middle: the ideas by status; Scan, Scan again, a Days field, a
  Direction field, Decline, Plan, Add (a title field).
- **Commands:** each button runs `release/PgCompilerBootstrap tools/writer/article.pg <args>` as a
  child process on a worker thread (the spawn code of `os.call`, shared as a source file the app
  compiles, as the tools do); its output lines reach the log as they come; when it ends, the app
  re-reads `article state` and reloads the files it changed. One command per article at a time;
  the others' buttons stay usable.
- **Files:** opened in the editor; Ctrl+S saves; an autosave 2 s after the last edit; a file
  changed on disk (checked every second) reloads when the editor has no unsaved edit, else a bar
  asks "changed on disk: reload / keep mine". Before a command that writes a file open with
  unsaved edits, the app saves it first.
- **Preview:** checks `http://localhost:4321/`; when nothing answers, starts `npm run dev` in the
  site folder (a child process stopped when the app closes) and waits for it; then opens
  `http://localhost:4321/preview/<slug>/` (with `?draft=` when a draft is open) with `xdg-open`.
- **Where it looks:** the site from `$ARTICLE_SITE` or `/home/corse/Documents/AstroTest`, the engine
  from where the app runs (the repository root), both shown in the top bar's tooltip.

## Left to the builder

- The widget's internal structure, its colours' theme keys, fonts and sizes.
- How a pipeline chip is drawn, the exact spacing of the window, which existing UI pieces to reuse.
- How the app reads `article state` (it is JSON: the engine's `json.hpp`).
- A demo scene for the widget, if useful.
- An escape for `\` in text shown through TTFText's markup (`\n`, `\c{`), so chapter text is drawn
  as written.
- The child's output reaching the log line by line, and what happens when the app closes while a
  command runs (the command is left to finish or stopped: the builder's choice, said in the log).
- When `plan.md` cannot be read mid-edit, the pipeline keeps showing the last state that could.

## Where this fits

(1) analytics, live; (2) PgScript modules, built; (3) the `article` tool, built and used;
(5) this; then (4) visuals in PgScript (charts, diagrams, the mind map as a third screen of this
app) and (6) live PgScript code boxes in articles.
