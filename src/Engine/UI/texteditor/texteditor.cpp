#include "stdafx.h"

#include "texteditor.h"

#include "Input/input.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

namespace pg
{
    using texteditor::SpanKind;
    using texteditor::TextPos;

    namespace
    {
        constexpr float DoubleClickMs = 400.0f;
        constexpr float BlinkMs = 500.0f;
        constexpr float UndoPauseMs = 1000.0f;
        constexpr size_t WheelLines = 3;
        constexpr float Padding = 6.0f;

        const constant::Vector4D BackgroundColour{30.0f, 31.0f, 36.0f, 255.0f};
        const constant::Vector4D GutterColour{36.0f, 38.0f, 44.0f, 255.0f};
        const constant::Vector4D SelectionColour{58.0f, 74.0f, 122.0f, 255.0f};
        const constant::Vector4D MatchColour{107.0f, 74.0f, 0.0f, 255.0f};
        const constant::Vector4D CurrentMatchColour{160.0f, 112.0f, 0.0f, 255.0f};
        const constant::Vector4D CursorColour{240.0f, 240.0f, 245.0f, 255.0f};
        const constant::Vector4D FindBarColour{44.0f, 46.0f, 54.0f, 255.0f};
        const texteditor::Rgba LineNumberColour{90, 95, 110, 255};
        const texteditor::Rgba FindTextColour{220, 220, 228, 255};

        texteditor::Rgba colourOf(SpanKind kind)
        {
            switch (kind)
            {
                case SpanKind::Heading:      return {130, 175, 255, 255};
                case SpanKind::Bold:         return {245, 245, 250, 255};
                case SpanKind::Italic:       return {200, 200, 218, 255};
                case SpanKind::InlineCode:   return {160, 205, 160, 255};
                case SpanKind::Fence:        return {115, 120, 135, 255};
                case SpanKind::CodeBlock:    return {150, 156, 172, 255};
                case SpanKind::Link:         return {120, 190, 232, 255};
                case SpanKind::ListMarker:   return {232, 180, 90, 255};
                case SpanKind::WriterMarker: return {255, 210, 122, 255};
                case SpanKind::Plain:
                default:                     return {220, 220, 228, 255};
            }
        }

        // A string drawn plainly (its backslashes escaped), for measuring as it is drawn
        std::string plainMarkup(const std::string& s)
        {
            return texteditor::lineMarkup(s, 0, s.size(), {}, [](SpanKind) { return texteditor::Rgba{255, 255, 255, 255}; });
        }

        bool inside(float x, float y, float rx, float ry, float rw, float rh)
        {
            return x >= rx and x < rx + rw and y >= ry and y < ry + rh;
        }
    }

    // ------------------------------------------------------------------------------- helpers

    double TextEditorSystem::nowMs()
    {
        using namespace std::chrono;
        return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
    }

    bool TextEditorSystem::isFocused(_unique_id id)
    {
        auto ent = ecsRef->getEntity(id);
        if (not ent or not ent->has<FocusableComponent>())
            return false;
        return ent->get<FocusableComponent>()->focused;
    }

    TextEditorComponent* TextEditorSystem::focusedEditor()
    {
        for (auto editor : view<TextEditorComponent>())
            if (isFocused(editor->entityId))
                return editor;
        return nullptr;
    }

    texteditor::MeasureFn TextEditorSystem::measureFor(TextEditorComponent* editor)
    {
        auto* ttf = ecsRef->getSystem<TTFTextSystem>();
        const std::string font = editor->font;
        const float scale = editor->scale;
        return [ttf, font, scale](const std::string& s) -> float {
            if (not ttf or s.empty())
                return 0.0f;
            return ttf->measureText(font, plainMarkup(s), scale).penEnd;
        };
    }

    TextEditorSystem::Geometry TextEditorSystem::geometryOf(_unique_id id, TextEditorComponent* editor)
    {
        Geometry g;
        auto ent = ecsRef->getEntity(id);
        if (not ent or not ent->has<PositionComponent>())
            return g;
        auto pos = ent->get<PositionComponent>();
        g.x = pos->x;
        g.y = pos->y;
        g.width = pos->width;
        g.height = pos->height;

        auto measure = measureFor(editor);
        if (auto* ttf = ecsRef->getSystem<TTFTextSystem>())
        {
            const float lh = ttf->measureText(editor->font, "Ag", editor->scale).lineHeight;
            if (lh > 0.0f)
                g.lineHeight = lh;
        }
        g.gutterWidth = editor->showLineNumbers ? measure("0000") + 2 * Padding : 0.0f;
        const float findHeight = editor->findOpen ? g.lineHeight + Padding : 0.0f;
        g.textX = g.x + g.gutterWidth + Padding;
        g.textY = g.y + findHeight + Padding;
        g.textWidth = std::max(20.0f, g.width - g.gutterWidth - 2 * Padding);
        const float rows = (g.height - findHeight - 2 * Padding) / g.lineHeight;
        g.visibleLines = rows >= 1.0f ? static_cast<size_t>(rows) : 1;
        return g;
    }

    void TextEditorSystem::edited(TextEditorComponent* editor)
    {
        editor->dirtyFlag = true;
        editor->needsRefresh = true;
        editor->followCursor = true;
        editor->wantedX = -1.0f;
        editor->lastEditMs = nowMs();
        editor->undoBroken = false;
        if (editor->findOpen)
            refreshMatches(editor, false);
        cursorOn = true;
        blinkStartMs = nowMs();
        ecsRef->sendEvent(TextEditorChanged{editor->entityId});
    }

    void TextEditorSystem::layout(TextEditorComponent* editor, const Geometry& g)
    {
        const auto& lines = editor->buffer.lines();
        if (editor->laidOutRevision != editor->buffer.revision() or editor->laidOutWidth != g.textWidth)
        {
            editor->wrap.rebuild(lines, g.textWidth, measureFor(editor));
            editor->spans = texteditor::markdownSpans(lines);
            editor->laidOutRevision = editor->buffer.revision();
            editor->laidOutWidth = g.textWidth;
        }
        const size_t total = editor->wrap.visual().size();
        const size_t maxTop = total > g.visibleLines ? total - g.visibleLines : 0;
        if (editor->topVisual > maxTop)
            editor->topVisual = maxTop;
    }

    void TextEditorSystem::keepCursorInView(TextEditorComponent* editor, const Geometry& g)
    {
        const size_t v = editor->wrap.visualOf(editor->buffer.cursor());
        if (v < editor->topVisual)
            editor->topVisual = v;
        else if (v >= editor->topVisual + g.visibleLines)
            editor->topVisual = v + 1 - g.visibleLines;
    }

    void TextEditorSystem::moveVertically(TextEditorComponent* editor, long lines, bool extend)
    {
        auto g = geometryOf(editor->entityId, editor);
        layout(editor, g);
        auto measure = measureFor(editor);
        const auto& text = editor->buffer.lines();
        const TextPos cursor = editor->buffer.cursor();
        if (editor->wantedX < 0.0f)
            editor->wantedX = editor->wrap.xOf(cursor, text, measure);
        const long current = static_cast<long>(editor->wrap.visualOf(cursor));
        const long last = static_cast<long>(editor->wrap.visual().size()) - 1;
        long target = current + lines;
        if (target < 0)
        {
            editor->buffer.setCursor({0, 0}, extend);
            return;
        }
        if (target > last)
        {
            editor->buffer.docEnd(extend);
            return;
        }
        const float x = editor->wantedX;
        editor->buffer.setCursor(editor->wrap.posAt(static_cast<size_t>(target), x, text, measure), extend);
        editor->wantedX = x;
    }

    TextPos TextEditorSystem::positionAt(TextEditorComponent* editor, float px, float py)
    {
        auto g = geometryOf(editor->entityId, editor);
        layout(editor, g);
        long row = static_cast<long>(std::floor((py - g.textY) / g.lineHeight));
        if (row < 0)
            row = 0;
        size_t v = editor->topVisual + static_cast<size_t>(row);
        if (editor->wrap.visual().empty())
            return {0, 0};
        if (v >= editor->wrap.visual().size())
            v = editor->wrap.visual().size() - 1;
        return editor->wrap.posAt(v, px - g.textX, editor->buffer.lines(), measureFor(editor));
    }

    void TextEditorSystem::refreshMatches(TextEditorComponent* editor, bool jump)
    {
        editor->matches = texteditor::findAll(editor->buffer.lines(), editor->query);
        editor->currentMatch = 0;
        if (jump and not editor->matches.empty())
        {
            auto from = editor->buffer.cursor();
            if (from.column > 0)
                from.column -= 1;
            jumpToMatch(editor, false);
        }
        editor->needsRefresh = true;
    }

    void TextEditorSystem::jumpToMatch(TextEditorComponent* editor, bool backwards)
    {
        if (editor->matches.empty())
            return;
        TextPos from = editor->buffer.hasSelection() ? editor->buffer.selection().first : editor->buffer.cursor();
        auto m = texteditor::nextMatch(editor->matches, from, backwards);
        if (not m)
            return;
        for (size_t i = 0; i < editor->matches.size(); ++i)
            if (editor->matches[i].line == m->line and editor->matches[i].start == m->start)
                editor->currentMatch = i;
        editor->buffer.setCursor({m->line, m->start}, false);
        editor->buffer.setCursor({m->line, m->end}, true);
        editor->followCursor = true;
        editor->needsRefresh = true;
    }

    // ------------------------------------------------------------------------------- input

    void TextEditorSystem::onEvent(const OnSDLTextInput& event)
    {
        auto editor = focusedEditor();
        if (not editor or ctrlDown)
            return;
        if (editor->findOpen)
        {
            editor->query += event.text;
            refreshMatches(editor, true);
            return;
        }
        if (editor->readOnly)
            return;
        editor->buffer.insert(event.text);
        edited(editor);
    }

    void TextEditorSystem::onEvent(const OnSDLScanCodeReleased& event)
    {
        if (event.key == SDL_SCANCODE_LSHIFT or event.key == SDL_SCANCODE_RSHIFT)
            shiftDown = false;
        if (event.key == SDL_SCANCODE_LCTRL or event.key == SDL_SCANCODE_RCTRL)
            ctrlDown = false;
    }

    void TextEditorSystem::onEvent(const OnSDLScanCode& event)
    {
        const bool ctrl = event.mod & KMOD_CTRL;
        const bool shift = event.mod & KMOD_SHIFT;
        if (event.key == SDL_SCANCODE_LSHIFT or event.key == SDL_SCANCODE_RSHIFT)
            shiftDown = true;
        if (event.key == SDL_SCANCODE_LCTRL or event.key == SDL_SCANCODE_RCTRL)
            ctrlDown = true;

        auto editor = focusedEditor();
        if (not editor)
            return;
        auto& b = editor->buffer;
        const bool canEdit = not editor->readOnly;
        bool moved = false;

        if (ctrl and event.key == SDL_SCANCODE_F)
        {
            editor->findOpen = true;
            if (b.hasSelection() and b.selectedText().find('\n') == std::string::npos)
                editor->query = b.selectedText();
            refreshMatches(editor, false);
            return;
        }

        if (editor->findOpen)
        {
            switch (event.key)
            {
                case SDL_SCANCODE_ESCAPE:
                    editor->findOpen = false;
                    editor->matches.clear();
                    editor->needsRefresh = true;
                    return;
                case SDL_SCANCODE_RETURN:
                case SDL_SCANCODE_KP_ENTER:
                    jumpToMatch(editor, shift);
                    return;
                case SDL_SCANCODE_BACKSPACE:
                    if (not editor->query.empty())
                    {
                        size_t cut = editor->query.size() - 1;
                        while (cut > 0 and (static_cast<unsigned char>(editor->query[cut]) & 0xC0) == 0x80)
                            --cut;
                        editor->query.erase(cut);
                        refreshMatches(editor, true);
                    }
                    return;
                default:
                    return;
            }
        }

        switch (event.key)
        {
            case SDL_SCANCODE_LEFT:      b.left(ctrl, shift); moved = true; break;
            case SDL_SCANCODE_RIGHT:     b.right(ctrl, shift); moved = true; break;
            case SDL_SCANCODE_UP:        moveVertically(editor, -1, shift); editor->followCursor = true; editor->needsRefresh = true; return;
            case SDL_SCANCODE_DOWN:      moveVertically(editor, 1, shift); editor->followCursor = true; editor->needsRefresh = true; return;
            case SDL_SCANCODE_PAGEUP:
            case SDL_SCANCODE_PAGEDOWN:
            {
                auto g = geometryOf(editor->entityId, editor);
                const long page = static_cast<long>(g.visibleLines > 1 ? g.visibleLines - 1 : 1);
                moveVertically(editor, event.key == SDL_SCANCODE_PAGEUP ? -page : page, shift);
                editor->followCursor = true;
                editor->needsRefresh = true;
                return;
            }
            case SDL_SCANCODE_HOME:
                if (ctrl)
                    b.docStart(shift);
                else
                {
                    auto g = geometryOf(editor->entityId, editor);
                    layout(editor, g);
                    const auto& v = editor->wrap.visual()[editor->wrap.visualOf(b.cursor())];
                    b.setCursor({v.line, v.start}, shift);
                }
                moved = true;
                break;
            case SDL_SCANCODE_END:
                if (ctrl)
                    b.docEnd(shift);
                else
                {
                    auto g = geometryOf(editor->entityId, editor);
                    layout(editor, g);
                    const size_t vi = editor->wrap.visualOf(b.cursor());
                    const auto& visual = editor->wrap.visual();
                    const auto& v = visual[vi];
                    const bool lastOfLine = vi + 1 >= visual.size() or visual[vi + 1].line != v.line;
                    size_t column = v.end;
                    // The end of a wrapped visual line is the next one's start: stay on this one
                    if (not lastOfLine and column > v.start)
                        column = b.prevBoundary(v.line, column);
                    b.setCursor({v.line, column}, shift);
                }
                moved = true;
                break;
            case SDL_SCANCODE_A:
                if (ctrl) { b.selectAll(); moved = true; }
                break;
            case SDL_SCANCODE_C:
                if (ctrl and b.hasSelection())
                    Clipboard::set(b.selectedText());
                return;
            case SDL_SCANCODE_X:
                if (ctrl and b.hasSelection())
                {
                    Clipboard::set(b.selectedText());
                    if (canEdit)
                    {
                        b.deleteSelection();
                        edited(editor);
                    }
                }
                return;
            case SDL_SCANCODE_V:
                if (ctrl and canEdit)
                    Clipboard::requestGet(editor->entityId);
                return;
            case SDL_SCANCODE_Z:
                if (ctrl and canEdit and (shift ? b.redo() : b.undo()))
                    edited(editor);
                return;
            case SDL_SCANCODE_Y:
                if (ctrl and canEdit and b.redo())
                    edited(editor);
                return;
            case SDL_SCANCODE_S:
                if (ctrl)
                    ecsRef->sendEvent(TextEditorSaveRequested{editor->entityId});
                return;
            case SDL_SCANCODE_BACKSPACE:
                if (canEdit) { b.backspace(ctrl); edited(editor); }
                return;
            case SDL_SCANCODE_DELETE:
                if (canEdit) { b.del(ctrl); edited(editor); }
                return;
            case SDL_SCANCODE_RETURN:
            case SDL_SCANCODE_KP_ENTER:
                if (canEdit) { b.newLine(); edited(editor); }
                return;
            case SDL_SCANCODE_TAB:
                if (canEdit) { b.insert("    "); edited(editor); }
                return;
            default:
                return;
        }

        if (moved)
        {
            editor->wantedX = -1.0f;
            editor->followCursor = true;
            editor->needsRefresh = true;
            cursorOn = true;
            blinkStartMs = nowMs();
        }
    }

    void TextEditorSystem::onEvent(const OnMouseClick& event)
    {
        if (event.button != SDL_BUTTON_LEFT)
            return;
        for (auto editor : view<TextEditorComponent>())
        {
            auto g = geometryOf(editor->entityId, editor);
            if (not inside(event.pos.x, event.pos.y, g.x, g.y, g.width, g.height))
                continue;
            const TextPos p = positionAt(editor, event.pos.x, event.pos.y);
            auto& b = editor->buffer;
            const double now = nowMs();
            if (now - editor->lastClickTime < DoubleClickMs and not shiftDown)
            {
                b.setCursor({p.line, b.wordLeft(p.line, b.nextBoundary(p.line, p.column))}, false);
                b.setCursor({p.line, b.wordRight(p.line, p.column)}, true);
                editor->lastClickTime = -10000.0;
            }
            else
            {
                b.setCursor(p, shiftDown);
                editor->lastClickTime = now;
                editor->dragging = true;
            }
            editor->wantedX = -1.0f;
            editor->needsRefresh = true;
            cursorOn = true;
            blinkStartMs = nowMs();
        }
    }

    void TextEditorSystem::onEvent(const OnMouseMove& event)
    {
        mouse = event.pos;
        const bool held = event.inputHandler and event.inputHandler->isButtonPressed(SDL_BUTTON_LEFT);
        for (auto editor : view<TextEditorComponent>())
        {
            if (not editor->dragging)
                continue;
            if (not held)
            {
                editor->dragging = false;
                continue;
            }
            editor->buffer.setCursor(positionAt(editor, event.pos.x, event.pos.y), true);
            editor->followCursor = true;
            editor->needsRefresh = true;
        }
    }

    void TextEditorSystem::onEvent(const OnMouseRelease&)
    {
        for (auto editor : view<TextEditorComponent>())
            editor->dragging = false;
    }

    void TextEditorSystem::onEvent(const OnSDLMouseWheel& event)
    {
        for (auto editor : view<TextEditorComponent>())
        {
            auto g = geometryOf(editor->entityId, editor);
            if (not inside(mouse.x, mouse.y, g.x, g.y, g.width, g.height))
                continue;
            layout(editor, g);
            const long step = static_cast<long>(WheelLines) * event.y;
            const long top = static_cast<long>(editor->topVisual) - step;
            editor->topVisual = top < 0 ? 0 : static_cast<size_t>(top);
            layout(editor, g);
            editor->needsRefresh = true;
        }
    }

    void TextEditorSystem::onEvent(const ClipboardText& event)
    {
        auto ent = ecsRef->getEntity(event.requester);
        if (not ent or not ent->has<TextEditorComponent>())
            return;
        auto editor = ent->get<TextEditorComponent>();
        if (editor->readOnly or event.text.empty())
            return;
        editor->buffer.insert(event.text);
        edited(editor);
    }

    void TextEditorSystem::onEvent(const TickEvent&)
    {
        const double now = nowMs();
        if (now - blinkStartMs >= BlinkMs)
        {
            blinkStartMs = now;
            cursorOn = not cursorOn;
            if (auto editor = focusedEditor())
                editor->needsRefresh = true;
        }
        for (auto editor : view<TextEditorComponent>())
        {
            if (not editor->undoBroken and now - editor->lastEditMs >= UndoPauseMs)
            {
                editor->buffer.breakUndoGroup();
                editor->undoBroken = true;
            }
        }
    }

    void TextEditorSystem::onEvent(const OnFocus&)
    {
        for (auto editor : view<TextEditorComponent>())
            editor->needsRefresh = true;
        cursorOn = true;
        blinkStartMs = nowMs();
    }

    // ------------------------------------------------------------------------------- drawing

    // The pooled shapes and texts are never hidden: an unused one is drawn empty (no text, no size).
    // Toggling their visibility each refresh left a shown cursor undrawn in the demo
    _unique_id TextEditorSystem::makeRect(_unique_id clipper, float z, constant::Vector4D colour)
    {
        auto rect = makeSimple2DShape(ecsRef, Shape2D::Square, 0.0f, 0.0f, colour);
        rect.get<PositionComponent>()->setZ(z);
        ecsRef->attach<ClippedTo>(rect.entity, clipper);
        return rect.entity.id;
    }

    _unique_id TextEditorSystem::makeText(_unique_id clipper, float z, const std::string& font, float scale)
    {
        auto text = makeTTFText(ecsRef, 0.0f, 0.0f, z, font, "", scale);
        ecsRef->attach<ClippedTo>(text.entity, clipper);
        return text.entity.id;
    }

    void TextEditorSystem::placeRect(_unique_id id, float x, float y, float w, float h, bool visible, const constant::Vector4D* colour)
    {
        auto ent = ecsRef->getEntity(id);
        if (not ent)
            return;
        auto pos = ent->get<PositionComponent>();
        if (not pos)
            return;
        if (not visible)
        {
            w = 0.0f;
            h = 0.0f;
        }
        if (pos->x != x) pos->setX(x);
        if (pos->y != y) pos->setY(y);
        if (pos->width != w) pos->setWidth(w);
        if (pos->height != h) pos->setHeight(h);
        if (colour and ent->has<Simple2DObject>())
        {
            auto shape = ent->get<Simple2DObject>();
            const auto c = shape->getColors();
            if (c.x != colour->x or c.y != colour->y or c.z != colour->z or c.w != colour->w)
                shape->setColors(*colour);
        }
    }

    void TextEditorSystem::placeText(_unique_id id, float x, float y, const std::string& markup, std::string* last, bool visible)
    {
        auto ent = ecsRef->getEntity(id);
        if (not ent)
            return;
        auto pos = ent->get<PositionComponent>();
        auto ttf = ent->get<TTFText>();
        if (not pos or not ttf)
            return;
        const std::string shown = visible ? markup : std::string();
        if (visible)
        {
            if (pos->x != x) pos->setX(x);
            if (pos->y != y) pos->setY(y);
        }
        if (last)
        {
            if (*last != shown)
            {
                ttf->setText(shown);
                *last = shown;
            }
        }
        else if (ttf->text != shown)
        {
            ttf->setText(shown);
        }
    }

    void TextEditorSystem::draw(TextEditorComponent* editor, const Geometry& g)
    {
        const _unique_id self = editor->entityId;
        auto selfEnt = ecsRef->getEntity(self);
        if (not selfEnt)
            return;
        const float z = selfEnt->get<PositionComponent>()->z;

        if (not editor->background)
            editor->background = makeRect(self, z, BackgroundColour);
        if (not editor->gutter)
            editor->gutter = makeRect(self, z + 1, GutterColour);
        if (not editor->cursorRect)
            editor->cursorRect = makeRect(self, z + 4, CursorColour);
        if (not editor->findBar)
            editor->findBar = makeRect(self, z + 1, FindBarColour);
        if (not editor->findText)
            editor->findText = makeText(self, z + 3, editor->font, editor->scale);

        placeRect(editor->background, g.x, g.y, g.width, g.height, true);
        placeRect(editor->gutter, g.x, g.y, g.gutterWidth, g.height, editor->showLineNumbers);

        // Find bar
        const float findHeight = editor->findOpen ? g.lineHeight + Padding : 0.0f;
        placeRect(editor->findBar, g.x, g.y, g.width, findHeight, editor->findOpen);
        if (editor->findOpen)
        {
            std::string label = "Find: " + editor->query;
            if (not editor->query.empty())
                label += editor->matches.empty() ? "   (no match)" : "   " + std::to_string(editor->currentMatch + 1) + "/" + std::to_string(editor->matches.size());
            const std::string markup = texteditor::lineMarkup(label, 0, label.size(), {}, [](SpanKind) { return FindTextColour; });
            placeText(editor->findText, g.x + Padding, g.y + Padding / 2, markup, nullptr, true);
        }
        else
        {
            placeText(editor->findText, 0, 0, "", nullptr, false);
        }

        // Lines in view
        const auto& lines = editor->buffer.lines();
        const auto& visual = editor->wrap.visual();
        while (editor->lineTexts.size() < g.visibleLines + 1)
        {
            editor->lineTexts.push_back(makeText(self, z + 3, editor->font, editor->scale));
            editor->lineMarkups.emplace_back();
            editor->numberTexts.push_back(makeText(self, z + 3, editor->font, editor->scale));
            editor->numberMarkups.emplace_back();
        }
        auto measure = measureFor(editor);
        for (size_t i = 0; i < editor->lineTexts.size(); ++i)
        {
            const size_t vi = editor->topVisual + i;
            const bool shown = i < g.visibleLines + 1 and vi < visual.size();
            if (not shown)
            {
                placeText(editor->lineTexts[i], 0, 0, "", &editor->lineMarkups[i], false);
                placeText(editor->numberTexts[i], 0, 0, "", &editor->numberMarkups[i], false);
                continue;
            }
            const auto& v = visual[vi];
            const float y = g.textY + static_cast<float>(i) * g.lineHeight;
            const auto& spans = v.line < editor->spans.size() ? editor->spans[v.line] : std::vector<texteditor::Span>{};
            placeText(editor->lineTexts[i], g.textX, y, texteditor::lineMarkup(lines[v.line], v.start, v.end, spans, colourOf), &editor->lineMarkups[i], true);

            const bool first = v.start == 0;
            if (editor->showLineNumbers and first)
            {
                const std::string number = std::to_string(v.line + 1);
                const float nx = g.x + g.gutterWidth - Padding - measure(number);
                placeText(editor->numberTexts[i], nx, y, texteditor::lineMarkup(number, 0, number.size(), {}, [](SpanKind) { return LineNumberColour; }), &editor->numberMarkups[i], true);
            }
            else
            {
                placeText(editor->numberTexts[i], 0, 0, "", &editor->numberMarkups[i], false);
            }
        }

        // Highlights: the selection, then the matches, on the visual lines in view
        struct Box { float x, y, w, h; const constant::Vector4D* colour; };
        std::vector<Box> boxes;
        auto addRange = [&](TextPos from, TextPos to, const constant::Vector4D* colour) {
            for (size_t i = 0; i <= g.visibleLines; ++i)
            {
                const size_t vi = editor->topVisual + i;
                if (vi >= visual.size())
                    break;
                const auto& v = visual[vi];
                const TextPos vStart{v.line, v.start};
                const TextPos vEnd{v.line, v.end};
                if (vEnd < from or to < vStart or (to == vStart and not (from == to)))
                    continue;
                const size_t a = from.line == v.line ? std::max(from.column, v.start) : v.start;
                const size_t c = to.line == v.line ? std::min(to.column, v.end) : v.end;
                if ((v.line < from.line) or (v.line > to.line))
                    continue;
                const float x0 = measure(lines[v.line].substr(v.start, a - v.start));
                float x1 = measure(lines[v.line].substr(v.start, c - v.start));
                // A selection going past this line's end shows a little of the break
                if (to.line > v.line and c == v.end)
                    x1 += g.lineHeight / 3;
                if (x1 <= x0)
                    continue;
                boxes.push_back({g.textX + x0, g.textY + static_cast<float>(i) * g.lineHeight, x1 - x0, g.lineHeight, colour});
            }
        };
        for (size_t m = 0; m < editor->matches.size(); ++m)
        {
            const auto& match = editor->matches[m];
            addRange({match.line, match.start}, {match.line, match.end}, m == editor->currentMatch ? &CurrentMatchColour : &MatchColour);
        }
        if (editor->buffer.hasSelection())
        {
            auto [from, to] = editor->buffer.selection();
            addRange(from, to, &SelectionColour);
        }
        while (editor->highlightRects.size() < boxes.size())
            editor->highlightRects.push_back(makeRect(self, z + 2, SelectionColour));
        for (size_t i = 0; i < editor->highlightRects.size(); ++i)
        {
            if (i < boxes.size())
                placeRect(editor->highlightRects[i], boxes[i].x, boxes[i].y, boxes[i].w, boxes[i].h, true, boxes[i].colour);
            else
                placeRect(editor->highlightRects[i], 0, 0, 0, 0, false);
        }

        // The cursor
        const TextPos cursor = editor->buffer.cursor();
        const size_t cv = editor->wrap.visualOf(cursor);
        const bool inView = cv >= editor->topVisual and cv < editor->topVisual + g.visibleLines + 1;
        // Blinks by its alpha: hiding and showing a shape each half second is not reliably redrawn
        const bool showCursor = inView and isFocused(self) and not editor->findOpen;
        const float cx = g.textX + editor->wrap.xOf(cursor, lines, measure);
        const float cy = g.textY + static_cast<float>(cv >= editor->topVisual ? cv - editor->topVisual : 0) * g.lineHeight;
        constant::Vector4D cursorColour = CursorColour;
        cursorColour.w = cursorOn ? 255.0f : 0.0f;
        placeRect(editor->cursorRect, cx, cy, 2.0f, g.lineHeight, showCursor, &cursorColour);
    }

    void TextEditorSystem::execute()
    {
        for (auto editor : view<TextEditorComponent>())
        {
            auto g = geometryOf(editor->entityId, editor);
            const bool resized = editor->laidOutWidth != g.textWidth;
            if (not editor->needsRefresh and not resized and editor->laidOutRevision == editor->buffer.revision())
                continue;
            layout(editor, g);
            if (editor->followCursor)
            {
                keepCursorInView(editor, g);
                editor->followCursor = false;
            }
            draw(editor, g);
            editor->needsRefresh = false;
        }
    }
}
