# Writer App, Part 2: the Text Editor Widget — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A multi-line Markdown text editor widget in the engine (`TextEditorComponent` + `TextEditorSystem`), its logic in a plain C++ model tested without a window, and a small demo app to try it.

**Architecture:** `src/Engine/UI/texteditor/` holds the model (buffer, undo, wrap, Markdown spans, search, line markup), pure C++ with a measure function injected. The ECS widget draws only the visual lines in view, one `TTFText` each with `\c{r,g,b,a}` colour runs, clipped with `ClippedTo` to its box, with rectangles for cursor, selection and matches; it keeps its own scroll offset. The SDL clipboard is reached through a main-thread service in `Window` (SDL expects it on the video thread; systems run on worker threads).

**Tech Stack:** C++17, the engine's ECS/UI (TTFText, Simple2DObject, UiAnchor, ClippedTo, FocusableComponent, Timer/TickEvent), SDL2, GoogleTest (`t1`).

**Spec:** `/docs/superpowers/specs/2026-11-16-writer-app-design.md` (Part 2).

## Global Constraints

- Builds in `/release` with `make -j4 <target>` (never a bare `-j`); the engine's tests are the `t1` target (`release/t1`).
- Branch `Chronicle`; commit only this plan's files; parallel sessions share the tree.
- New `.cpp` files go into the full engine's source list (`ColumbaEngine`, next to `src/Engine/UI/textinput.cpp`), not the minimal one; the model files include nothing of the ECS or SDL.
- Columns are byte offsets into UTF-8 lines and never fall inside a code point (a continuation byte is `(c & 0xC0) == 0x80`).
- TTFText markup facts: `\c{r,g,b,a}` (0–255) sets the colour until the next one; a literal backslash followed by `n` is drawn as a line break and one followed by `c{` starts a colour: the line markup escapes every backslash (`\` then a colour tag, then the rest), so text is drawn as written. Tabs have no glyph: Tab inserts 4 spaces.
- No monospace font in the repo: the demo registers Inter (`res/font/Inter/static/Inter_24pt-Regular.ttf` as `"editor"`); x positions always come from `measureText(font, prefix, scale).penEnd` (the same as `TextInputSystem::updateCursorVisual`).
- Do not attach `TextInputComponent` to the editor (TextInputSystem would also handle its keys).
- Events from another thread go through `ecsRef->sendEvent` (thread-safe), never `System::sendEvent`.

## Review Focus

1. Text with backslashes (`\n`, `\c{1,2,3,4}`, a trailing `\`) and with `{`/`}` is drawn exactly as written (Task 4 markup tests).
2. UTF-8 (é, —, emoji) at a cursor move, a backspace, a word move, a wrap point and a selection edge: never a broken code point (Tasks 1, 3).
3. Undo after a paste of many lines, after a selection replaced by typing, and across a cut: the buffer returns exactly to before (Task 2).
4. A very long line with no space (a URL, a code line): wrap breaks it anyway, nothing drawn past the box (Task 3).
5. A fence never closed: the rest of the file is code, and closing it later restores the colouring of everything after (Task 4).

---

### Task 1: TextBuffer — lines, cursor, selection, edits, moves

**Files:** Create `src/Engine/UI/texteditor/textbuffer.h`, `textbuffer.cpp`; Create `test/texteditor.cc`; Modify `CMakeLists.txt` (the source, and the test in `t1`'s sources).

**Interfaces — Produces** (namespace `pg::texteditor`):
- `struct TextPos { size_t line = 0, column = 0; }` with `==`, `<`.
- `class TextBuffer`: `TextBuffer(const std::string& text = "")`; `std::string text() const` (lines joined with `\n`; a `\r\n` in the input becomes `\n`); `const std::vector<std::string>& lines() const`; `TextPos cursor() const`; `std::optional<TextPos> anchor() const` (selection start, none when no selection); `bool hasSelection() const`; `std::pair<TextPos,TextPos> selection() const` (ordered); `std::string selectedText() const`; `void setCursor(TextPos, bool extend)` (clamped; `extend` keeps or sets the anchor, else clears it).
- Edits (each replaces the selection first when there is one): `insert(const std::string&)` (may hold `\n`), `newLine()`, `backspace(bool word)`, `del(bool word)`, `deleteSelection()`.
- Moves, each `(bool extend)`: `left(word)`, `right(word)`, `up()`, `down()` (keep a wanted column in bytes across lines; the widget overrides with visual moves), `lineStart()`, `lineEnd()`, `docStart()`, `docEnd()`, `selectAll()`.
- Word rule: a word is a run of letters/digits/`_`/non-ASCII; Ctrl+Left goes to the start of the word before the cursor (skipping spaces and punctuation first), Ctrl+Right to the end of the next word.
- `size_t revision() const` (grows on every edit).

- [ ] **Step 1: Write the failing tests** (`TEST(TextBuffer, ...)`)

```cpp
EmptyBufferHasOneEmptyLine            // TextBuffer b; lines().size()==1, text()==""
CrLfBecomesLf                         // TextBuffer b("a\r\nb"); lines()=={"a","b"}
InsertAndNewLine                      // insert("ab"), newLine(), insert("c") -> "ab\nc", cursor {1,1}
InsertMultiLineText                   // insert("x\ny\nz") at {0,1} of "ab" -> "ax\ny\nzb", cursor {2,1}
BackspaceJoinsLines                   // "ab\ncd", cursor {1,0}, backspace -> "abcd", cursor {0,2}
BackspaceWord                         // "foo bar  ", cursor end, backspace(true) -> "foo "
DeleteAtLineEndJoins                  // "ab\ncd", cursor {0,2}, del -> "abcd"
Utf8NeverSplit                        // "é—😀": right() steps 2,3,4 bytes; backspace removes whole code points; setCursor({0,1}) clamps to 0 or 2
WordMoves                             // "foo, bar_baz é" : right(true) from 0 -> 3; again -> 12 (end of bar_baz); left(true) from end -> start of "é"
UpDownKeepColumn                      // "abcdef\nab\nabcdef", cursor {0,5}, down -> {1,2}, down -> {2,5}
SelectionByShift                      // "abc", right(true) twice -> selectedText "ab"; typing "X" -> "Xc", no selection
SelectAllAndDelete                    // selectAll, deleteSelection -> "", one empty line
SelectionAcrossLines                  // "ab\ncd", select {0,1}..{1,1} -> selectedText "b\nc"
```

- [ ] **Step 2: Run** `cd release && make -j4 t1 && ./t1 --gtest_filter='TextBuffer*'` — Expected: build error (no `textbuffer.h`).
- [ ] **Step 3: Implement** `TextBuffer`.
- [ ] **Step 4: Run** the same — Expected: all pass.
- [ ] **Step 5: Commit** `git commit -m "Text editor: the buffer, cursor, selection and edits"`.

### Task 2: Undo and redo

**Files:** Modify `textbuffer.h/.cpp`, `test/texteditor.cc`.

**Interfaces — Produces:** `bool undo()`, `bool redo()` (false when nothing to do; cursor and selection restored to before / after the group); `void breakUndoGroup()` (the widget calls it after a pause of 1 s without edits); grouping: consecutive single-character inserts of word characters, or consecutive single backspaces, join one group; any other edit, a move or `setCursor` closes the group; a new edit after an undo drops the redo history.

- [ ] **Step 1: Write the failing tests**

```cpp
TypingAWordIsOneUndo                  // insert "h","e","l","l","o" one by one; undo -> "", one call
SpaceStartsANewGroup                  // type "ab cd" char by char: undo -> "ab ", undo -> ""   (space closes the word's group and starts its own)
MoveBreaksTheGroup                    // type "ab", left(), type "c": undo -> "ab" with cursor {0,1}
PasteManyLinesIsOneUndo               // insert("1\n2\n3\n4") ; undo -> original, cursor back   (Review Focus 3)
ReplaceSelectionThenUndo              // "hello", select all, type "X": undo -> "hello" with "hello" selected again
CutThenUndo                           // select "ell", deleteSelection: undo -> "hello"
RedoAfterUndo                         // ... redo -> the edit again; a new edit after undo -> redo() false
BreakUndoGroupSplits                  // type "ab", breakUndoGroup(), type "c": undo -> "ab"
```

- [ ] **Step 2–4:** as Task 1 with `--gtest_filter='TextBuffer*'`.
- [ ] **Step 5: Commit** `git commit -m "Text editor: undo and redo by groups"`.

### Task 3: Wrapping

**Files:** Create `src/Engine/UI/texteditor/wrap.h`, `wrap.cpp`; Modify `test/texteditor.cc`, `CMakeLists.txt`.

**Interfaces — Produces:**
- `using MeasureFn = std::function<float(const std::string&)>;` (the pen x after a string).
- `struct VisualLine { size_t line, start, end; }` (bytes `[start, end)` of buffer line `line`).
- `class WrapLayout`: `void rebuild(const std::vector<std::string>& lines, float width, const MeasureFn&)`; `void rebuildLines(const std::vector<std::string>& lines, size_t first, size_t count, ...)` is NOT needed: instead `rebuild` keeps a per-line cache keyed by the line's text and the width, so unchanged lines are not measured again; `const std::vector<VisualLine>& visual() const`; `size_t visualOf(TextPos) const`; `float xOf(TextPos, const std::vector<std::string>&, const MeasureFn&) const` (x within its visual line); `TextPos posAt(size_t visualIndex, float x, const std::vector<std::string>&, const MeasureFn&) const` (nearest boundary, never inside a code point).
- Rule: break after the last space that fits; with no space that fits, break at the last code point that fits (at least one code point a visual line); an empty line is one empty visual line; trailing spaces stay on the line they end.

- [ ] **Step 1: Write the failing tests** (a fixed measure: 10 px a byte)

```cpp
ShortLinesDoNotWrap                   // {"abc","de"}, width 100 -> 2 visual lines
WrapsAtLastSpace                      // "aaa bbb ccc", width 75 -> "aaa ", "bbb ", "ccc"
LongWordBreaksAnywhere                // "https://example.com/a/very/long/path", width 100 -> every visual line <= 10 bytes, joined back equals the line   (Review Focus 4)
Utf8WrapPoint                         // "éééééé" (2 bytes each, measure 10/byte), width 50 -> breaks at even byte offsets only   (Review Focus 2)
EmptyLineIsOneVisualLine
XAndPosRoundTrip                      // for every boundary p of a wrapped text: posAt(visualOf(p), xOf(p)) == p
CacheSkipsUnchangedLines              // a counting measure: rebuild twice with one line changed -> the second rebuild measures only that line
```

- [ ] **Step 2–4:** `make -j4 t1 && ./t1 --gtest_filter='Wrap*'`.
- [ ] **Step 5: Commit** `git commit -m "Text editor: wrapping and position mapping"`.

### Task 4: Markdown spans, search, and line markup

**Files:** Create `src/Engine/UI/texteditor/markdown.h/.cpp`, `search.h/.cpp`, `markup.h/.cpp`; Modify `test/texteditor.cc`, `CMakeLists.txt`.

**Interfaces — Produces:**
- `enum class SpanKind { Plain, Heading, Bold, Italic, InlineCode, Fence, CodeBlock, Link, ListMarker, WriterMarker };`
- `struct Span { size_t start, end; SpanKind kind; };`
- `std::vector<std::vector<Span>> markdownSpans(const std::vector<std::string>& lines)` — spans cover each line in order with no gap (Plain fills); rules: a line starting with 1–6 `#` + space is all Heading; a line starting with ```` ``` ```` is a Fence and toggles code; lines inside are all CodeBlock (no other rule applies); `- `, `* `, `+ `, `N. ` at the line start (after spaces) are ListMarker; `` `...` `` InlineCode; `**...**` Bold; `*...*` / `_..._` Italic (not inside a word for `_`); `[text](url)` Link; `<!-- visual: ... -->` and `<!-- number: ... -->` WriterMarker (and an unclosed one runs to the line end).
- `struct Match { size_t line, start, end; };` `std::vector<Match> findAll(const std::vector<std::string>& lines, const std::string& query)` (ASCII case-insensitive; empty query → none); `std::optional<Match> nextMatch(const std::vector<Match>&, TextPos from, bool backwards)` (wraps around).
- `struct Rgba { uint8_t r, g, b, a; };` `std::string lineMarkup(const std::string& text, size_t start, size_t end, const std::vector<Span>& spans, const std::function<Rgba(SpanKind)>& colourOf)` — the bytes `[start,end)` with a `\c{r,g,b,a}` before each span's part, and each `\` written as `\` + `\c{same colour}` so TTFText draws it as written.

- [ ] **Step 1: Write the failing tests**

```cpp
HeadingLine                           // "## Two kinds" -> one Heading span 0..12
FenceAndCodeBlock                     // {"text","```cpp","int a;","```","after"} -> line 1 Fence, line 2 CodeBlock, line 3 Fence, line 4 Plain
UnclosedFenceRunsToEnd                // {"```","a","b"} -> lines 1,2 CodeBlock; adding "```" at the end -> line 3 Fence; then a line after is Plain   (Review Focus 5)
InlineCodeBoldItalicLink              // "a `x` **b** *c* [d](e)" -> the kinds at their byte ranges
UnderscoreInsideWordIsNotItalic       // "snake_case_name" -> all Plain
ListMarkers                           // "- a", "  * b", "12. c" -> ListMarker on the marker only
WriterMarkers                         // "x <!-- visual: chart --> y", "<!-- number: n" -> WriterMarker ranges
SpansCoverTheLine                     // for random lines: spans contiguous from 0 to size
FindAllIgnoresCase                    // {"Foo foo","FOO"} "foo" -> 3 matches; nextMatch from the last wraps to the first; backwards from first -> last
MarkupEscapesBackslashes              // lineMarkup("a\\nb\\c{1,2,3,4}c\\", ...) parsed by TTFTextSystem::measureText as one line (lineCount 1) and its visible text equals the input  (use the headless measure setup of test/ttftext.cc)   (Review Focus 1)
MarkupColoursSpans                    // "## x" with Heading -> starts with "\\c{" + heading colour + "}"
```

- [ ] **Step 2–4:** `make -j4 t1 && ./t1 --gtest_filter='Markdown*:Search*:Markup*'`.
- [ ] **Step 5: Commit** `git commit -m "Text editor: Markdown spans, search, line markup"`.

### Task 5: The clipboard service

**Files:** Create `src/Engine/Input/clipboard.h/.cpp`; Modify `src/Engine/window.cpp` (call `Clipboard::service(ecs)` once a frame on the main thread, in `Window::render()` before drawing), `CMakeLists.txt`, `test/texteditor.cc`.

**Interfaces — Produces:**
- `struct ClipboardText { _unique_id requester; std::string text; };` (event)
- `class Clipboard` (static, thread-safe with a mutex): `static void set(const std::string& text)`; `static void requestGet(_unique_id requester)`; `static void service(EntitySystem* ecs, const std::function<std::string()>& get = SDL_GetClipboardText wrapper, const std::function<void(const std::string&)>& set = SDL_SetClipboardText wrapper)` — performs the queued sets, then for each queued get sends `ClipboardText{requester, text}` with `ecs->sendEvent`.
- [ ] **Step 1: Write the failing test:** `ClipboardServiceRelays` — with fake get/set functions and an `EntitySystem` plus a `Listener<ClipboardText>` system: `set("a")` then `service` → the fake set received "a"; `requestGet(7)` then `service` then `ecs.executeOnce()` → the listener got `{7, <fake text>}`.
- [ ] **Step 2–4:** `make -j4 t1 && ./t1 --gtest_filter='Clipboard*'`.
- [ ] **Step 5: Commit** `git commit -m "Clipboard: a main-thread service for SDL's clipboard"`.

### Task 6: The widget and the demo

**Files:** Create `src/Engine/UI/texteditor/texteditor.h/.cpp` (component + system), `examples/TextEditorDemo/main.cpp`, `application.h/.cpp`; Modify `CMakeLists.txt` (source; target `TextEditorDemo` like `ShapesTest`, linking `ColumbaEngine`).

**Interfaces — Produces:**
- `struct TextEditorChanged { _unique_id id; };` `struct TextEditorSaveRequested { _unique_id id; };`
- `struct TextEditorComponent` (holds a `texteditor::TextBuffer`, `WrapLayout`, scroll offset in visual lines, font alias (default `"editor"`), scale (default 0.4), the find query, a dirty flag): `setText(const std::string&)` (also marks saved), `std::string text() const`, `bool dirty() const`, `void markSaved()`.
- `CompList<PositionComponent, UiAnchor, TextEditorComponent, FocusableComponent> makeTextEditor(EntitySystem*, float x, float y, float width, float height, const std::string& text = "")`.
- `TextEditorSystem : System<Own<TextEditorComponent>, Listener<OnSDLTextInput>, Listener<OnSDLScanCode>, Listener<OnMouseClick>, Listener<OnMouseMove>, Listener<OnMouseRelease>, Listener<OnSDLMouseWheel>, Listener<ClipboardText>, Listener<TickEvent>, Listener<ResizeEvent>, ...>`:
  - draws, for the focused or any editor, only the visual lines in view: a gutter of line numbers (only on a buffer line's first visual line), a `TTFText` a visual line with `lineMarkup` (colours: a fixed table of the widget's, overridable later by theme), the selection and match rectangles (z below text), the cursor (a 2 px rectangle, blinking every 500 ms, solid right after an edit), all children `ClippedTo` the editor; reuses line entities from a pool instead of making new ones each frame;
  - keys (from `event.mod`): arrows (Up/Down as visual moves through `WrapLayout`), Home/End (visual line), Ctrl+Home/End, PageUp/PageDown (the lines in view), Ctrl+Left/Right, Shift with any, Ctrl+A, Backspace/Delete (Ctrl: word), Enter, Tab (4 spaces), Ctrl+C/X/V (Clipboard), Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z, Ctrl+S (`TextEditorSaveRequested`), Ctrl+F (find), Escape (close find);
  - mouse: click places the cursor (`posAt`), Shift+click extends, drag selects (`OnMouseMove` with the left button down), a double click within 400 ms selects a word, the wheel scrolls 3 visual lines a notch; the view follows the cursor after every key;
  - find: Ctrl+F shows a one-line field above the text (the widget's own: typed characters go to the query while it is open), Enter / Shift+Enter jump to next / previous, all matches highlighted, the count shown ("2/5");
  - `breakUndoGroup` after 1 s without edits; `TextEditorChanged` after each edit.
- The demo: a window 1000×700, Inter registered as `"editor"`, one editor filling it with a sample Markdown text (headings, a fence, a list, markers, a long line, UTF-8, backslashes); Ctrl+S prints the text to stdout.

- [ ] **Step 1:** Build `make -j4 TextEditorDemo` — Expected: builds.
- [ ] **Step 2: Check by hand** (the author, or a screenshot if a display is reachable): type, wrap at the window edge, select with mouse and Shift, copy/paste with another app, undo/redo, scroll a long text, find, colours of each Markdown kind, backslashes drawn as written. Record what was checked in the ledger.
- [ ] **Step 3:** `make -j4 t1 && ./t1` — Expected: no new failures.
- [ ] **Step 4: Commit** `git commit -m "Text editor: the widget and a demo app"`.
